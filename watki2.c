#include <pthread.h>
#include <stdio.h>
#include <stdlib.h> // rand
#include <unistd.h> // sleep, usleep
#include <ncurses.h>

#define NUM_FOREST 2
#define NUM_LUMBERJACK 5
#define NUM_CAR 7

#define INITIAL_TREES 18000
#define INITIAL_WOOD 40

#define FINAL_TRANSPORTS 20


#define MAX_WOOD_STOCK 2000

#define MAX_AFFORESTATION 40 // trees on ar
#define MIN_AFFORESTATION 10 // trees on ar
#define FOREST_AREA 1000 // ars

#define TREE_TO_WOOD 10
#define LUMBERJACK_TREES 4
#define LUMBERJACK_TREES_GROW 10
#define FOREST_GROWTH 0.1
#define TRANSPORT_REQUIREMENT 20

#define DELAY_TIME 2000000

long trees = INITIAL_TREES;
long minTrees = FOREST_AREA * MIN_AFFORESTATION;
long maxTrees = FOREST_AREA * MAX_AFFORESTATION;
long wood = INITIAL_WOOD;
long minWood = 0;
long maxWood = MAX_WOOD_STOCK;
long forestGrowth = (double)(FOREST_GROWTH) * FOREST_AREA;

long craftedWood = TREE_TO_WOOD * LUMBERJACK_TREES;

volatile long transports = 0;
volatile long pendingTransports = 0;
volatile long runningForests = NUM_FOREST;
volatile long runningLumberjacks = NUM_LUMBERJACK;
volatile long runningCars = NUM_CAR;

pthread_mutex_t trees_mutex;
pthread_mutex_t wood_mutex;
pthread_cond_t trees_lower_max_cond;// when is lower than max, then forest can grow
pthread_cond_t trees_cut_cond;      // when is higher than min afforestation, lumberjack can cut
pthread_cond_t wood_max_cond;       // when is lower than max storage minus lumberjack single cut, lumberjack can cut
pthread_cond_t wood_car_cond;       // when is higher than min transport requirement, car can come

int numberOfThreads = NUM_FOREST + NUM_LUMBERJACK + NUM_CAR;
volatile int forestRun[NUM_FOREST];
volatile int lumberjackRun[NUM_LUMBERJACK];
volatile int carRun[NUM_CAR];

volatile int actualLumberjack[NUM_LUMBERJACK];
volatile int wholeLumberjack[NUM_LUMBERJACK];
volatile int actualCar[NUM_CAR];
volatile int wholeCar[NUM_CAR];

int kolumny = 0;
int rzedy = 0;
int xDisp = 0;
int yDisp = 0;
int yDispTrees = 0;
int yDispWood = 0;

int randomDelay() {
  srand(time(NULL));
  return DELAY_TIME + (rand() / (RAND_MAX + 1.0)) * (DELAY_TIME * 4 / 10);
}

void delay(int value) {
  usleep(value);
}

void delayOld() {
  srand(time(NULL));
  int value = DELAY_TIME + (rand() / (RAND_MAX + 1.0)) * (DELAY_TIME * 4 / 10);
  usleep(value);
}

void writeNumberGeneral(int count, int max, int y) {
  char writeArray[80] = {' '};

  count = (count * 80) / max;

  for (int i = 0; i < count; i++) {
    writeArray[i] = '#';
  }
  for (int i = count; i <= 80; i++) {
    writeArray[i] = ' ';
  }
  move(y, xDisp);
  printw(writeArray);
  clrtoeol();
  refresh();
  usleep(1000);
}

void writeNumber(int count, int y) {
  writeNumberGeneral(count, 80, y);
}

void writeTrees(int count) {
  writeNumberGeneral(count, maxTrees, yDispTrees);
}

void writeWood(int count) {
  writeNumberGeneral(count, maxWood, yDispWood);
}

void writeText0(char text[], int line) {
  move(line, xDisp);
  printw(text);
  clrtoeol();
  refresh();
  usleep(1000);
}

void writeText1(char text[], int arg1, int diff) {
  move(yDisp + diff, xDisp);
  printw(text, arg1);
  clrtoeol();
  refresh();
  usleep(1000);
}

void writeText2(char text[], int arg1, int arg2, int diff) {
  move(yDisp + diff, xDisp);
  printw(text, arg1, arg2);
  clrtoeol();
  refresh();
  usleep(1000);
}

void writeText3(char text[], int arg1, int arg2, int arg3, int diff) {
  move(0 + diff, xDisp);
  printw(text, arg1, arg2, arg3);
  clrtoeol();
  refresh();
  usleep(1000);
}

void writeText4(char text[], int arg1, int arg2, int arg3, int arg4, int diff) {
  move(0 + diff, xDisp);
  printw(text, arg1, arg2, arg3, arg4);
  clrtoeol();
  refresh();
  usleep(1000);
}

void writeText5(char text[], int arg1, int arg2, int arg3, int arg4, int arg5, int diff) {
  move(0 + diff, xDisp);
  printw(text, arg1, arg2, arg3, arg4, arg5);
  clrtoeol();
  refresh();
  usleep(1000);
}

void writeDescription(int line) {
  writeText0("forest                                stock                            transport", line);
}

void writeForest(int my_id) {
  writeText3("forest %ld grows trees: %d/%d.", my_id, trees, maxTrees, my_id);
}

bool ifWriteNewForest = false;

void writeLumberjack(int my_id) {
  writeText5("lumberjack %ld cut trees forest can grow trees: %d/%d, wood: %d/%d.", my_id, trees, maxTrees, wood, maxWood, NUM_FOREST + my_id);
}

void writeNewLumberjack(int my_id, int actual, int whole) {
  char tab[80];
  if (actual % 2 == 0) {
    tab[1] = tab[0] = '/';
  } else {
    tab[1] = tab[0] = '\\';
  }

  int lumberjack = (actual * (38 - 2)) / whole + 2;

  for (int i = 2; i < lumberjack; i++) {
    tab[i] = ' ';
  }
  tab[lumberjack] = 'o';

  for (int i = lumberjack + 1; i < 39; i++) {
    tab[i] = ' ';
  }

  tab[39] = tab[40] = tab[41] = '#';

  for (int i = 42; i < 80; i++) {
    tab[i] = ' ';
  }
  
  int offset = my_id + 1;
  if (ifWriteNewForest) {
    offset += NUM_FOREST;
  }
  writeText0(tab, offset);
}

void writeCar(int my_id) {
  writeText5("car %ld runs somewhere and leaves wood: %d/%d, transport: %d/%d.", my_id, wood, maxWood, transports, FINAL_TRANSPORTS, NUM_FOREST + NUM_LUMBERJACK + my_id);
}

void writeNewCar(int my_id, int actual, int whole) {
  char tab[80];
  if (actual % 2 == 0) {
    tab[1] = tab[0] = '/';
  } else {
    tab[1] = tab[0] = '\\';
  }

  for (int i = 2; i < 39; i++) {
    tab[i] = ' ';
  }

  tab[39] = tab[40] = tab[41] = '#';

  int car = (actual * (40 - 2)) / whole + 42;

  for (int i = 42; i < car; i++) {
    tab[i] = ' ';
  }
 
  tab[car] = 'x';

  for (int i = car + 1; i < 80; i++) {
    tab[i] = ' ';
  }
  int offset = NUM_LUMBERJACK + my_id + 1;
  if (ifWriteNewForest) {
    offset += NUM_FOREST;
  }
  writeText0(tab, offset);
}

void writeRunning(int i) {
  writeText4("%d: forest: %d, lumberjack: %d, car: %d.", i, runningForests, runningLumberjacks, runningCars, NUM_FOREST + NUM_LUMBERJACK + NUM_CAR);
}

void writeRaw(int i) {
  writeText5("trees: %d/%d, wood: %d/%d, transports: %d", trees, maxTrees, wood, maxWood, transports, i);
}

void *writeThread(void *t) {
//  int onceAgain = 0;
  while (runningCars > 0) {// || onceAgain < numberOfThreads) {
    /*for (long i = 0; i < NUM_FOREST; i++) {
      writeForest(i);
    }
    for (long i = 0; i < NUM_LUMBERJACK; i++) {
      writeLumberjack(i);
    }
    for (long i = 0; i < NUM_CAR; i++) {
      writeCar(i);
    }*/
    //writeRunning(onceAgain);
    writeRaw(0);
    writeDescription(1);
    for (long i = 0; i < NUM_LUMBERJACK; i++) {
      writeNewLumberjack(i + 1, actualLumberjack[i], wholeLumberjack[i]);
    }
    for (long i = 0; i < NUM_CAR; i++) {
      writeNewCar(i + 1, actualCar[i], wholeCar[i]);
    }
    usleep(DELAY_TIME / 50);

//    if (runningCars == 0 && onceAgain < numberOfThreads) {
//      onceAgain++;
//    }
  }
  pthread_exit(NULL);
}

void *forestThread(void *t) {
  long my_id = (long)t;
  while (runningCars > 0) {
    pthread_mutex_lock(&trees_mutex);

    if (trees + forestGrowth > maxTrees) {
      pthread_cond_wait(&trees_lower_max_cond, &trees_mutex);
    }

    if (transports + pendingTransports < FINAL_TRANSPORTS) {
      trees += forestGrowth;
    }

    if (trees >= minTrees + LUMBERJACK_TREES || transports + pendingTransports >= FINAL_TRANSPORTS) {
      pthread_cond_signal(&trees_cut_cond);
    }

    pthread_mutex_unlock(&trees_mutex);

    delayOld();
    forestRun[my_id]++;
  }
  runningForests--; 
  usleep(100);
  pthread_exit(NULL);
}

// !!! (LUMBERJACK_TREES + LUMBERJACK_TREES_GROW <= maxTrees - minTrees) !!!
void *lumberjackThread(void *t) {
  long my_id = (long)t;

  while (runningCars > 0) {
    pthread_mutex_lock(&trees_mutex);
    // Check whether to wait
    if (trees < minTrees + LUMBERJACK_TREES) {
      // Run lumberjack to grow trees.
      if (trees + LUMBERJACK_TREES_GROW <= maxTrees) {

        if (transports + pendingTransports < FINAL_TRANSPORTS) {
          trees += LUMBERJACK_TREES_GROW;
        }

        // Check condition to signalize
        if (trees + forestGrowth <= maxTrees || transports + pendingTransports >= FINAL_TRANSPORTS) {
          pthread_cond_signal(&trees_lower_max_cond);
        }

        pthread_mutex_unlock(&trees_mutex);
        pthread_mutex_unlock(&wood_mutex);
      }

      delayOld();
    } else {

      // Run task to cut trees
      if (transports + pendingTransports < FINAL_TRANSPORTS) {
        actualLumberjack[my_id] = 0;
        wholeLumberjack[my_id] = 1;

        trees -= LUMBERJACK_TREES;

        int ranDelay = randomDelay() / 5;
        delay(ranDelay);
      }

     // Check condition to signalize
      if (trees + forestGrowth <= maxTrees || transports + pendingTransports >= FINAL_TRANSPORTS) {
        pthread_cond_signal(&trees_lower_max_cond);
      }

      // Unlock resource
      pthread_mutex_unlock(&trees_mutex);

      if (transports + pendingTransports < FINAL_TRANSPORTS) {
        int ranDelay = randomDelay();
        actualLumberjack[my_id] = 0;
        wholeLumberjack[my_id] = ranDelay;
        int singleDelay = ranDelay / 200;
        for (int i = 0; i < 195; i++) {
          delay(singleDelay);
          actualLumberjack[my_id] += singleDelay;
        }
        for (int i = 195; i < 200; i++) {
          delay(singleDelay);
        }
      }

      pthread_mutex_lock(&wood_mutex);

      if (wood + craftedWood > maxWood) {
        pthread_cond_wait(&wood_max_cond, &wood_mutex);
      }

      actualLumberjack[my_id] = 100;
      wholeLumberjack[my_id] = 100;
      
      if (transports + pendingTransports < FINAL_TRANSPORTS) {
        wood += craftedWood;

        int ranDelay = randomDelay() / 5;
        delay(ranDelay);
      }

      if (wood >= TRANSPORT_REQUIREMENT || transports + pendingTransports >= FINAL_TRANSPORTS) {
        pthread_cond_signal(&wood_car_cond);
      }

      pthread_mutex_unlock(&wood_mutex);

      if (transports + pendingTransports < FINAL_TRANSPORTS) {
        int ranDelay = randomDelay();
        wholeLumberjack[my_id] = ranDelay;
        actualLumberjack[my_id] = ranDelay;
        int singleDelay = ranDelay / 200;
        for (int i = 0; i < 190; i++) {
          delay(singleDelay);
          actualLumberjack[my_id] -= singleDelay;
        }
        for (int i = 190; i < 200; i++) {
          delay(singleDelay);
        }
      }
    }

    lumberjackRun[my_id]++;
  }

  runningLumberjacks--;
  usleep(100);
  pthread_exit(NULL);
}

void *carThread(void *t) {
  long my_id = (long)t;
  while (transports + pendingTransports < FINAL_TRANSPORTS) {
    if (transports + pendingTransports < FINAL_TRANSPORTS) {
      int ranDelay = randomDelay();
      wholeCar[my_id] = ranDelay;
      actualCar[my_id] = ranDelay;
      int singleDelay = ranDelay / 200;
      for (int i = 0; i < 190; i++) {
        delay(singleDelay);
        actualCar[my_id] -= singleDelay;
      }
      for (int i = 190; i < 200; i++) {
        delay(singleDelay);
      }
    }

    pthread_mutex_lock(&wood_mutex);

    if (wood < TRANSPORT_REQUIREMENT) {
      pthread_cond_wait(&wood_car_cond, &wood_mutex);
    }

    wholeCar[my_id] = 100;
    actualCar[my_id] = 0;

    if (wood >= TRANSPORT_REQUIREMENT && transports + pendingTransports < FINAL_TRANSPORTS) {
        pendingTransports++;

        int ranDelay = randomDelay() / 5;
        delay(ranDelay);

        wood -= TRANSPORT_REQUIREMENT;
    }

    if (wood + craftedWood <= maxWood || transports + pendingTransports >= FINAL_TRANSPORTS) {
      pthread_cond_signal(&wood_max_cond);
    }

    pthread_mutex_unlock(&wood_mutex);

    if (transports + pendingTransports < FINAL_TRANSPORTS) {
      int ranDelay = randomDelay();
      wholeCar[my_id] = ranDelay;
      actualCar[my_id] = 0;
      int singleDelay = ranDelay / 200;
      for (int i = 0; i < 200; i++) {
        delay(singleDelay);
        actualCar[my_id] += singleDelay;
      }
    }

    int ranDelay = randomDelay() / 5;
    delay(ranDelay);

    pendingTransports--;
    transports = transports + 1;
    carRun[my_id] = carRun[my_id] + 1;
  }

  runningCars--;
  usleep(100);
  pthread_exit(NULL);
}

int main (int argc, char *argv[]) {
  initscr();
  getmaxyx(stdscr, rzedy, kolumny);
  xDisp = 0;
  yDisp = rzedy / 2;
  yDispTrees = yDisp + 2;
  yDispWood = yDisp + 4;

  pthread_t threads[numberOfThreads];
  pthread_t writingThread;
  pthread_attr_t attr;

  pthread_mutex_init(&trees_mutex, NULL);
  pthread_mutex_init(&wood_mutex, NULL);
  pthread_cond_init(&trees_lower_max_cond, NULL);
  pthread_cond_init(&trees_cut_cond, NULL);
  pthread_cond_init(&wood_max_cond, NULL);
  pthread_cond_init(&wood_car_cond, NULL); 

  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

  int threadCounter = 0;
  for (long i = 0; i < NUM_FOREST; i++) {
    pthread_create(&threads[threadCounter++], &attr, forestThread, (void *)i);
    forestRun[i] = 0;
  }
  for (long i = 0; i < NUM_LUMBERJACK; i++) {
    pthread_create(&threads[threadCounter++], &attr, lumberjackThread, (void *)i);
    lumberjackRun[i] = 0;
    actualLumberjack[i] = 5;
    wholeLumberjack[i] = 100;
  }
  for (long i = 0; i < NUM_CAR; i++) {
    pthread_create(&threads[threadCounter++], &attr, carThread, (void *)i);
    carRun[i] = 0;
    actualCar[i] = 1;
    wholeCar[i] = 1;
  }

  pthread_create(&writingThread, &attr, writeThread, NULL);
  pthread_join(writingThread, NULL);

  for (int i = 0; i < numberOfThreads; i++) {
    pthread_join(threads[i], NULL);
  }

  usleep(1);

  writeText1("Main(): Waited on %d threads. Done.", numberOfThreads, 0);
  for (int i = 0; i < NUM_FOREST; i++) {
    writeText2("forest %d runned: %d times", i, forestRun[i], i);
  }
  for (int i = 0; i < NUM_LUMBERJACK; i++) {
    writeText2("lumberjack %d runned: %d times", i, lumberjackRun[i], NUM_FOREST + i);
  }
  for (int i = 0; i < NUM_CAR; i++) {
    writeText2("car %d runned: %d times", i, carRun[i], NUM_FOREST + NUM_LUMBERJACK + i);
  }
  writeText1("transports: %d", transports, NUM_FOREST + NUM_LUMBERJACK + NUM_CAR);

  pthread_attr_destroy(&attr);
  pthread_mutex_destroy(&trees_mutex);
  pthread_mutex_destroy(&wood_mutex);
  pthread_cond_destroy(&trees_lower_max_cond);
  pthread_cond_destroy(&trees_cut_cond);
  pthread_cond_destroy(&wood_max_cond);
  pthread_cond_destroy(&wood_car_cond);
  getch();
  endwin();
  pthread_exit(NULL);
}
