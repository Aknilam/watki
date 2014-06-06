#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> // sleep, usleep
#include <ncurses.h>

#define NUM_THREADS 4
#define TCOUNT 20
#define COUNT_LIMIT 12


#define NUM_FOREST 1
#define NUM_LUMBERJACK 1
#define NUM_CAR 5

#define INITIAL_TREES 9000
#define INITIAL_WOOD 400

#define FINAL_TRANSPORTS 50


#define MAX_WOOD_STOCK 2000

#define MAX_AFFORESTATION 40 // trees on ar
#define MIN_AFFORESTATION 10 // trees on ar
#define FOREST_AREA 1000 // ars

#define TREE_TO_WOOD 10
#define LUMBERJACK_TREES 4
#define LUMBERJACK_TREES_GROW 10
#define FOREST_GROWTH 0.1
#define TRANSPORT_REQUIREMENT 20


long trees = INITIAL_TREES;
long minTrees = FOREST_AREA * MIN_AFFORESTATION;
long maxTrees = FOREST_AREA * MAX_AFFORESTATION;
long wood = INITIAL_WOOD;
long minWood = 0;
long maxWood = MAX_WOOD_STOCK;
long forestGrowth = (double)(FOREST_GROWTH) * FOREST_AREA;

long craftedWood = TREE_TO_WOOD * LUMBERJACK_TREES;

volatile long transports = 0;
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


int kolumny = 0;
int rzedy = 0;
int xDisp = 0;
int yDisp = 0;
int yDispTrees = 0;
int yDispWood = 0;

void delay() {
  usleep(100000);
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

void writeText0(char text[]) {
  move(yDisp, xDisp);
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

void writeText5(char text[], int arg1, int arg2, int arg3, int arg4, int arg5, int diff) {
  move(0 + diff, xDisp);
  printw(text, arg1, arg2, arg3, arg4, arg5);
  clrtoeol();
  refresh();
  usleep(1000);
}

void writeForest(int my_id) {
  writeText3("forest %ld grows trees: %d/%d.", my_id, trees, maxTrees, my_id);
  //writeTrees(trees);
  //writeWood(wood);
}

void writeLumberjack(int my_id) {
  writeText5("lumberjack %ld cut trees forest can grow trees: %d/%d, wood: %d/%d.", my_id, trees, maxTrees, wood, maxWood, NUM_FOREST + my_id);
  //writeTrees(trees);
  //writeWood(wood);
}

void writeCar(int my_id) {
  writeText5("car %ld runs somewhere and leaves wood: %d/%d, transport: %d/%d.", my_id, wood, maxWood, transports, FINAL_TRANSPORTS, NUM_FOREST + NUM_LUMBERJACK + my_id);
  //writeTrees(trees);
  //writeWood(wood);
}

void writeRunning() {
  writeText3("forest: %d, lumberjack: %d, car: %d.", runningForests, runningLumberjacks, runningCars, NUM_FOREST + NUM_LUMBERJACK + NUM_CAR);
}

void *forestThread(void *t) {
  long my_id = (long)t;
  pthread_mutex_lock(&trees_mutex);
  while (transports < FINAL_TRANSPORTS || runningCars > 0) {

    if (trees + forestGrowth > maxTrees) {
      pthread_cond_wait(&trees_lower_max_cond, &trees_mutex);
    }

    trees += forestGrowth;

    if (trees >= minTrees + LUMBERJACK_TREES) {
      pthread_cond_signal(&trees_cut_cond);
    }

    //writeForest(my_id);

    pthread_mutex_unlock(&trees_mutex);

    delay();
    forestRun[my_id]++;
  }
  runningForests--; 
  usleep(100);
  pthread_exit(NULL);
}

// !!! (LUMBERJACK_TREES + LUMBERJACK_TREES_GROW <= maxTrees - minTrees) !!!
void *lumberjackThread(void *t) {
  long my_id = (long)t;

  pthread_mutex_lock(&trees_mutex);
  pthread_mutex_lock(&wood_mutex);
  while (transports < FINAL_TRANSPORTS || runningCars > 0) {
    // Check whether to wait
    if (trees < minTrees + LUMBERJACK_TREES) {
      // Run lumberjack to grow trees.
      if (trees + LUMBERJACK_TREES_GROW <= maxTrees) {

        trees += LUMBERJACK_TREES_GROW;

        // Check condition to signalize
        if (trees + forestGrowth <= maxTrees) {
          pthread_cond_signal(&trees_lower_max_cond);
        }

        //writeLumberjack(my_id);

        pthread_mutex_unlock(&trees_mutex);
        pthread_mutex_unlock(&wood_mutex);

        delay();
      }
      //pthread_cond_wait(&trees_cut_cond, &trees_mutex);
    } else {

      // Run task to cut trees
      trees -= LUMBERJACK_TREES;

      // Check condition to signalize
      if (trees + forestGrowth <= maxTrees) {
        pthread_cond_signal(&trees_lower_max_cond);
      }

      // Write data
      //writeLumberjack(my_id);

      // Unlock resource
      pthread_mutex_unlock(&trees_mutex);

      delay();

      if (wood + craftedWood > maxWood) {
        pthread_cond_wait(&wood_max_cond, &wood_mutex);
      }
      wood += craftedWood;

      if (wood >= TRANSPORT_REQUIREMENT) {
        pthread_cond_signal(&wood_car_cond);
      }

      //writeLumberjack(my_id);
      pthread_mutex_unlock(&wood_mutex);

      delay();
    }

    lumberjackRun[my_id]++;
  }

  runningLumberjacks--;
  usleep(100);
  pthread_exit(NULL);
}

void *carThread(void *t) {
  long my_id = (long)t;
  pthread_mutex_lock(&wood_mutex);
  while (transports < FINAL_TRANSPORTS) {

    if (wood < TRANSPORT_REQUIREMENT) {
      pthread_cond_wait(&wood_car_cond, &wood_mutex);
    }

    int items = TRANSPORT_REQUIREMENT;
    wood -= items;
    transports = transports + 1;
    carRun[my_id] = carRun[my_id] + 1;

    if (wood + craftedWood <= maxWood) {
      pthread_cond_signal(&wood_max_cond);
    }

    //writeCar(my_id);
    pthread_mutex_unlock(&wood_mutex);

    delay();
  }

  runningCars--;
  usleep(100);
  pthread_exit(NULL);
}

void *writeThread(void *t) {
  while (transports < FINAL_TRANSPORTS) {
    for (long i = 0; i < NUM_FOREST; i++) {
      writeForest(i);
    }
    for (long i = 0; i < NUM_LUMBERJACK; i++) {
      writeLumberjack(i);
    }
    for (long i = 0; i < NUM_CAR; i++) {
      writeCar(i);
    }
    writeRunning();
    usleep(100000);
  }
  pthread_exit(NULL);
}



int count = 0;
pthread_mutex_t count_mutex;
pthread_cond_t count_threshold_cv;

int i;

void *inc_count(void *t) {
  long my_id = (long)t;

  for (i = 0; i < TCOUNT; i++) {
    pthread_mutex_lock(&count_mutex);
    count += 2;

    if (count >= COUNT_LIMIT) {
      pthread_cond_signal(&count_threshold_cv);
      writeText2("inc_count(): thread %ld, count = %d  Threshold reached.", my_id, count, 0);
    }
    writeText2("inc_count(): thread %ld, count = %d, unlocking mutex", my_id, count, 0);
    writeNumber(count, yDisp + 6);
    pthread_mutex_unlock(&count_mutex);

    sleep(1);
  }

  usleep(1);
  pthread_cond_signal(&count_threshold_cv);
  pthread_exit(NULL);
}

void *destroyer(void *t) {

  pthread_mutex_lock(&count_mutex);

  pthread_cond_wait(&count_threshold_cv, &count_mutex);
  count = 5;
  writeText0("inc_count(): destroyer destroyed");

  writeNumber(count, yDisp + 6);
  pthread_mutex_unlock(&count_mutex);
  pthread_exit(NULL);
}

void *watch_count(void *t) {
  long my_id = (long)t;

  writeText1("Starting watch_count(): thread %ld", my_id, 0);

  pthread_mutex_lock(&count_mutex);
  while (count<COUNT_LIMIT && i<TCOUNT-1) {
    pthread_cond_wait(&count_threshold_cv, &count_mutex);
    writeText1("watch_count(): thread %ld Condition signal received.", my_id, 0);
    count -= 10;
    writeText2("watch_count(): thread %ld count now = %d.", my_id, count, 0);
    sleep(1);
  }
  pthread_mutex_unlock(&count_mutex);
  writeNumber(count, yDisp + 6);
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
  }
  for (long i = 0; i < NUM_CAR; i++) {
    pthread_create(&threads[threadCounter++], &attr, carThread, (void *)i);
    carRun[i] = 0;
  }

  pthread_create(&writingThread, &attr, writeThread, NULL);
  pthread_join(writingThread, NULL);

  for (int i = 0; i < numberOfThreads; i++) {
    pthread_join(threads[i], NULL);
  }


/*
  long t1 = 1, t2 = 2, t3 = 3;
  pthread_t threads[4];
  pthread_attr_t attr;

  pthread_mutex_init(&count_mutex, NULL);
  pthread_cond_init (&count_threshold_cv, NULL);

  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
  pthread_create(&threads[0], &attr, watch_count, (void *)t1);
  pthread_create(&threads[1], &attr, inc_count, (void *)t2);
  pthread_create(&threads[2], &attr, inc_count, (void *)t3);
  pthread_create(&threads[3], &attr, destroyer, NULL);


  for (int i=0; i<NUM_THREADS; i++) {
    pthread_join(threads[i], NULL);
  }*/
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