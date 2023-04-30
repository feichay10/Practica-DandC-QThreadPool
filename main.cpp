#include <QCoreApplication>
#include <QVector>
#include <iostream>
#include <fstream>
#include <getopt.h>
#include <math.h>
#include <QThread> ///< Function QThread::idealThreadCount()
#include <QThreadPool>
#include <QRunnable>
#include <QElapsedTimer>
#include <atomic>
#include <mutex>
#include <unistd.h>
#include <sched.h>

// https://stackoverflow.com/questions/7988486/how-do-you-calculate-the-variance-median-and-standard-deviation-in-c-or-java/7988556#7988556

#define INVALID_ARG 1
#define INVALID_INVOCATION 2
#define HELP_ASKED 3

const char* PROG_NAME = "DC_THREADPOOL";
const char* MANUAL_PATH = "manual.hlp";

unsigned int actual_time_executed{0}; ///< It helps us to show the actual time of execution when we execute some times the program
bool show_data = false;
const int kSecondsInADay{86400};
unsigned int total = 60 * 60 * 24 * 365; ///< sec * min * hour * days
std::atomic<int> days_processed{0}; ///< Used in ThreadPoolMode
QVector<std::pair<double, double>> MeanAndMedianDaysInAYear(365);
const unsigned totalBufferSize = 60 * 60 * 24 * 30;
QVector<float> Buffer;

QElapsedTimer timer; ///< It will be our timer to count the time in each iteration of each method to solve the problem
qint64 last_time = Q_INT64_C(0); ///< We set to zero
qint64 old_measurement = -1;


void ShowHelp() {
  std::cout << "This program does calculations of median and average in a set of random temperature measurements." << std::endl;
  std::cout << "Invoking modes are as follows (in any order):" << std::endl;
  std::cout << "  > --help / -h # Shows these instructions" << std::endl;
  std::cout << "  > --pool-of-threads / -p VALUE # Runs program in ThreadPool mode with VALUE indicating number of threads" << std::endl;
  std::cout << "  > --divide-and-conquer / -d VALUE # Runs program in D&C mode with VALUE indicating number of divisions" << std::endl;
  std::cout << "  > --number-of-exec / -n TIMES # Executes the last indicated method (-p/-d) the number of times written" << std::endl;
  std::cout << "  > --real-time-priority / -r PRIORITY # Gives real-time priority based on range 1-99 (the greater the value, the better)" << std::endl;
  std::cout << "Notes: '#' indicates a comment. Using -p and -d together is allowed; last one will be considered" << std::endl;
  std::cout << "Real-time priority is determined as -1 - value, so if 99 is entered maximum priority -100 is given " << std::endl;
}

/// This class allows us to save in a single vector, all the temperature measurements in each second of a processor and apply statistical calculations to those measurements.
class Statistics {
 public:
  Statistics(const QVector<float>& data) {
    this->data = data;
    size = data.length();
  }

  double getMean(void) {
    double sum = 0.0;

    for(double a : data) sum += a;

    return sum / size;
  }

  double median(void) {
    std::sort(data.begin(), data.end());

    if (data.length() % 2 == 0)
      return (data[(data.length() / 2) - 1] + data[data.length() / 2]) / 2.0;

    return data[data.length() / 2];
  }

 private:
  QVector<float> data; ///< In each position we store a temperature measurement
  int size; ///< We save the vector size to speed up operations
};

/// this functions allows us to compare the performance against using threads in the other two methods
void SerialMode(void) {
  std::cout << "Serial mode start => Actual iteration: " << ++actual_time_executed << std::endl;

  if (show_data) std::cout << "------------------------------------------------------------------------------" << std::endl;

  last_time = timer.nsecsElapsed(); ///< We run the chronometer
  QVector<float> d(kSecondsInADay); ///< Here we save all the measurements of a single day
  int actual_day{0};

  for(unsigned int i{0}; i <= total; ++i) { ///< We go through all the seconds that a year has
    d[i % (kSecondsInADay)] = (random() % 50 + 50); ///< We put a random temperature measurement in the position that corresponds to the vector

    if(i % (kSecondsInADay) == 0) { ///< If we fill in all the data for a day, we make the measurements.
      Statistics s(d);
      double mean{s.getMean()};
      double median{s.median()};

      if (show_data)
        std::cout << ++actual_day << " of 365 - Average: " << mean << " - Median: " << median << std::endl;
    }
  }

  last_time = timer.nsecsElapsed() - last_time;  ///< We stop the chronometer

  if (show_data) {
    std::cout << "------------------------------------------------------------------------------" << std::endl;
    std::cout << "Done in serial mode - Time of this iteration: " << ((double)(last_time) / 1000000000) << " seconds" << std::endl << std::endl;
  }
}

class ThreadPoolTask : public QRunnable {
 public:
  void run() override {
    int actual_day{days_processed++}; ///< We use the post-increment to do it in a single line
    QVector<float> d(kSecondsInADay); ///< Here we save all the measurements of a single day

    for(int i{0}; i < kSecondsInADay; ++i) {
      d[i % kSecondsInADay] = (random() % 50 + 50); ///< We put a random temperature measurement in the position that corresponds to the vector
    }

    Statistics s(d);
    MeanAndMedianDaysInAYear[actual_day].first = s.getMean();
    MeanAndMedianDaysInAYear[actual_day].second = s.median();
  }
};

void ThreadPoolMode(int num_threads) {
  /// if num_threads is greater than the threads available in the system
  /// num_threads will be equal to the value of the free threads in the system
  // if (num_threads > unsigned(QThread::idealThreadCount())) num_threads = QThread::idealThreadCount(); ///< Uncomment to better performance
  if (num_threads < 1) num_threads = 1;

  QThreadPool thread_pool;
  thread_pool.setMaxThreadCount(num_threads);
  std::cout << "Thread pool mode start => Threads: " << thread_pool.maxThreadCount() << " - Actual iteration: " << ++actual_time_executed << std::endl;
  last_time = timer.nsecsElapsed(); ///< We run the chronometer

  for(int i{0}; i < 365; ++i) { ///< We add a task to QThreadPool for each day of the year
    ThreadPoolTask* task = new ThreadPoolTask();
    thread_pool.start(task);
  }

  thread_pool.waitForDone(); ///< Wait for all tasks to finish before exiting

  if (show_data) {
    int actual_day{1};
    std::cout << "------------------------------------------------------------------------------" << std::endl;

    for(auto i : MeanAndMedianDaysInAYear)
      std::cout << actual_day++ << " of 365 - Average: " << i.first << " - Median: " << i.second << std::endl;

    std::cout << "------------------------------------------------------------------------------" << std::endl;
    last_time = timer.nsecsElapsed() - last_time; ///< We stop the chronometer
    std::cout << "Done in Thread pool mode - Time of this iteration: " << ((double)(last_time) / 1000000000) << " seconds" << std::endl << std::endl;

  } else {
    last_time = timer.nsecsElapsed() - last_time; ///< We stop the chronometer
  }

  days_processed = 0; ///< If we want to use this mood again, we must reset the critical data.
}

void Procedure(unsigned begin, unsigned end) {
  QVector<float> d(kSecondsInADay); ///< Here we save all the measurements of a single day

  for(unsigned int i{begin}; i <= end; ++i) { ///< We go through all the seconds that a year has
    d[i % (kSecondsInADay)] = (random() % 50 + 50); ///< We put a random temperature measurement in the position that corresponds to the vector

    if(i % (kSecondsInADay) == 0) { ///< If we fill in all the data for a day, we make the measurements.
      Statistics s(d);
      MeanAndMedianDaysInAYear[(i / kSecondsInADay) - 1].first = s.getMean();
      MeanAndMedianDaysInAYear[(i / kSecondsInADay) - 1].second = s.median();
    }
  }
}

void DivideAndConquerCreaterThreads(unsigned begin, unsigned end, unsigned depth = 0) {
  if ((end - begin > kSecondsInADay)) {
    const unsigned pivot = ((((begin + end) / 2) % kSecondsInADay != 0) ? (((begin + end) / 2) + (kSecondsInADay / 2)) : ((begin + end) / 2));

    if (pivot < total - kSecondsInADay) {
      if (depth > 0) {  ///< If we have not reached the maximum depth, we create two threads
        std::thread t1(DivideAndConquerCreaterThreads, begin, pivot, depth - 1);
        std::thread t2(DivideAndConquerCreaterThreads, pivot, end, depth - 1);
        t1.join();
        t2.join();

      } else {  ///< Else we create one thread
        std::thread t1(Procedure, begin, end);
        t1.join();
      }
    }
  }
}

void DivideAndConquerMode(int num_recursive_calls) {
  last_time = timer.nsecsElapsed(); ///< We run the chronometer

  if (num_recursive_calls <  0) num_recursive_calls = 0;

  std::cout << "Divide and conquer mode start => Threads: " << pow(2, num_recursive_calls)
            << " - Actual iteration: " << ++actual_time_executed << std::endl;
  std::thread t1(DivideAndConquerCreaterThreads, 1, total, num_recursive_calls);
  t1.join();  ///< We execute the selected mode going to the concret function

  if(show_data) {
    int actual_day{1};
    std::cout << "------------------------------------------------------------------------------" << std::endl;

    for(auto i : MeanAndMedianDaysInAYear)
      std::cout << actual_day++ << " of 365 - Average: " << i.first << " - Median: " << i.second << std::endl;

    std::cout << "------------------------------------------------------------------------------" << std::endl;
    std::cout << "Done in Divide and conquer mode" << std::endl << std::endl;
  }

  last_time = timer.nsecsElapsed() - last_time; ///< We stop the chronometer
  days_processed = 0;
}

int main(int argc, char* argv[]) {
  QCoreApplication a(argc, argv);

  try {
    Buffer.resize(total);
    struct sched_param param { ///Contains priority from 1-99 for real-time measurements
      99
    };
    ///Process command line parameters -> help
    struct option long_options[] = { ///< argument for an option is stored in optarg (if it was optional_argument, and was not specified, will be a nullptr)
      {"help", no_argument, NULL, 'h'}, ///< option 0 -> help / no arg / no flag / char 'h' which identifies the option (and is also the equivalent short option for convenience)
      {"pool-of-threads", required_argument, NULL, 'p'}, ///< option 1 -> ThreadPool / arg (value) / no flag / char 'p' which identifies the option (and is also the equivalent short option for convenience)
      {"divide-and-conquer", required_argument, NULL, 'd'}, ///< option 2 -> D&C / arg (value) / no flag / char 'd' which identifies the option (and is also the equivalent short option for convenience)
      {"number-of-exec", required_argument, NULL, 'n'}, ///< option 3 -> number of executions / arg (num_exec) / no flag / char 'n' which identifies the option (and is also the equivalent short option for convenience)
      {"show-data", no_argument, NULL, 's'}, ///< option 4 -> show temperature data / no arg / no flag / char 's' which identifies the option (and is also the equivalent short option for convenience)
      {"real-time-priority", required_argument, NULL, 'r'}, ///< option 5 -> real time / arg(priority 1-99) / no flag / char 'r' which identifies the option (and is also the equivalent short option for convenience)
      {0, 0, 0, 0} ///< Explicit array termination
    };
    unsigned int number_of_executions{1};
    unsigned int value{0}; ///< Value is for ThreadPool and D&Q versions (thread subdivision and list divisions, respectively)
    char selected_mode = ' ';

    while (true) { ///< if there were arguments after the options, they would need to be processed from optind which indicates index of next element in argv[]
      int option_index = 0;
      char opt_identifier = getopt_long(argc, argv, "hp:d:n:sr:", long_options, &option_index); ///< after an option means required argument

      if (opt_identifier == -1) break; ///< No more options of either size

      switch (opt_identifier) {
        case 'h':
          ShowHelp();
          return HELP_ASKED;

        case 'p': ///< ThreadPool
          selected_mode = 'p';
          value = std::stoi(optarg);
          break;

        case 'd': ///< Divide & Conquer
          selected_mode = 'd';
          value = std::stoi(optarg);
          break;

        case 'n': ///< Several executions
          number_of_executions = std::stoi(optarg);
          break;

        case 's': ///< Show debugging text
          show_data = true;
          break;

        case 'r': { ///< Set REAL-TIME-PRIORITY if possible
          int rt_priority = std::stoi(optarg);

          ///We could use either SCHED_FIFO or SCHED_RR (Round Robin) as exclusive modes for real-time
          if (rt_priority >= sched_get_priority_min(SCHED_RR) && rt_priority <= sched_get_priority_max(SCHED_RR)) {
            param.sched_priority = rt_priority; ///1-99 value that will be substracted from -1 to get [-1,100] priority]
            pid_t pid = getpid();

            if (sched_setscheduler(pid, SCHED_RR, &param) != 0) {
              std::cerr << "Could not set RT priority. Check permissions" << std::endl;
              return INVALID_INVOCATION;
            }

          } else {
            std::cerr << "RT priority needs to be in range: " << sched_get_priority_min(SCHED_RR) << ' ' << sched_get_priority_max(SCHED_RR) << std::endl;
            return INVALID_INVOCATION;
          }

          break;
        }

        default:
          std::cerr << "Unrecognized command-line parameter..." << std::endl;
          return INVALID_INVOCATION; ///getoptlong already shows an error
      }
    }

    if (number_of_executions < 1 || value < 0)
      return INVALID_INVOCATION;

    for (int i = 0; i < number_of_executions; ++i) {
      switch (selected_mode) {
        case 'p': {
          ThreadPoolMode(value);
          break;
        }

        case 'd': {
          DivideAndConquerMode(value);
          break;
        }

        default: {
          SerialMode(); ///< We execute the selected mode going to the concret function
          break;
        }
      };

      if (last_time < old_measurement || old_measurement == -1) old_measurement = last_time;
    }

    actual_time_executed = 0; ///< We reset the variable what tell to us in which iteration we was
    /// Show best and last time for the chosen execution mode
    std::cout << std::endl << "Last " << ((selected_mode == 'p') ? "Thread Pool" : ((selected_mode == 'd') ? "D&C" : "Serial")) << " time: "
              << (double)(last_time) / 1000000000 << " seconds - Minimum: "
              << (double)(old_measurement) / 1000000000
              << ", Times Executed: " << number_of_executions << std::endl;

  } catch (std::exception& error) {
    std::cerr << PROG_NAME << " > EXCEPTION > " << error.what() << std::endl;
    return INVALID_ARG;
  }

  return 0;
}
