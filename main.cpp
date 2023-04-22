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

// https://stackoverflow.com/questions/7988486/how-do-you-calculate-the-variance-median-and-standard-deviation-in-c-or-java/7988556#7988556

#define INVALID_ARG 1
#define INVALID_INVOCATION 2
#define HELP_ASKED 3

const char* PROG_NAME = "DC_THREADPOOL";
const char* MANUAL_PATH = "manual.hlp";

const int kSecondsInADay{86400};
unsigned int total = 60 * 60 * 24 * 365; ///< sec * min * hour * days
std::atomic<int> days_processed{0}; ///< Used in ThreadPoolMode
std::mutex mtx_screen; ///< We control the access to screen with this mutex in ThreadPoolMode

QElapsedTimer timer; ///< It will be our timer to count the time in each iteration of each method to solve the problem
qint64 time_serial = Q_INT64_C(0); ///< We set to zero
qint64 time_thread_pool = Q_INT64_C(0); ///< We set to zero
qint64 time_divide_and_conqueror = Q_INT64_C(0); ///< We set to zeros
std::vector<std::pair<double, int>> acc_num_measurements = {std::pair<double, int>(0.0, 0), std::pair<double, int>(0.0, 0), std::pair<double, int>(0.0, 0)};


void ShowHelp() {
  std::cout << "This program does calculations of median and average in a set of random temperature measurements." << std::endl;
  std::cout << "Invoking modes are as follows:" << std::endl;
  std::cout << "  > --help / -h # Shows these instructions" << std::endl;
  std::cout << "  > --pool-of-threads / -p VALUE # Runs program in ThreadPool mode with VALUE indicating number of threads" << std::endl;
  std::cout << "  > --divide-and-conquer / -d VALUE # Runs program in D&C mode with VALUE indicating number of divisions" << std::endl;
  std::cout << "  > --number-of-exec / -n TIMES # Executes the last indicated method (-p/-d) the number of times written" << std::endl;
  std::cout << "Notes: '#' indicates a comment.";
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
  std::cout << "Serial mode start:" << std::endl;
  std::cout << "------------------------------------------------------------------------------" << std::endl;
  time_serial = timer.nsecsElapsed(); ///< We run the chronometer
  QVector<float> d(kSecondsInADay); ///< Here we save all the measurements of a single day
  for(unsigned int i{0}; i <= total; ++i) { ///< We go through all the seconds that a year has
    d[i % (kSecondsInADay)] = (random() % 50 + 50); ///< We put a random temperature measurement in the position that corresponds to the vector
    if(i % (kSecondsInADay) == 0) { ///< If we fill in all the data for a day, we make the measurements.
      Statistics s(d);
      std::cout << i << " of " << total << " " << "average temperature " << s.getMean()
                << " with median " << s.median() << std::endl;
    }
  }
  time_serial = timer.nsecsElapsed() - time_serial;  ///< We stop the chronometer
  std::cout << "------------------------------------------------------------------------------" << std::endl;
  std::cout << "Done in serial mode" << std::endl << std::endl;
}



class ThreadPoolTask : public QRunnable {
 public:
  void run() override {
    int actual_day{++days_processed};
    QVector<float> d(kSecondsInADay); ///< Here we save all the measurements of a single day
    for(int i{1}; i <= kSecondsInADay; ++i) {
      d[(i - 1) % (kSecondsInADay)] = (random() % 50 + 50); ///< We put a random temperature measurement in the position that corresponds to the vector
      if(i % (kSecondsInADay) == 0) { ///< If we fill in all the data for a day, we make the measurements.
          Statistics s(d);
          double mean{s.getMean()};
          double median{s.median()};
          mtx_screen.lock(); ///< We took the mutex to have one thread writing in the screen at the same time
          std::cout << actual_day << " of 365 - Average: " << mean << " - Median: " << median << std::endl;
          mtx_screen.unlock();
      }
    }
  }
};



void ThreadPoolMode(unsigned int num_threads) {
  std::cout << "Thread pool mode start:" << std::endl;
  std::cout << "------------------------------------------------------------------------------" << std::endl;
  time_thread_pool = timer.nsecsElapsed(); ///< We run the chronometer

  /// if num_threads is greater than the threads available in the system
  /// num_threads will be equal to the value of the free threads in the system
  if (num_threads > unsigned(QThread::idealThreadCount())) num_threads = QThread::idealThreadCount();

  QThreadPool thread_pool;
  thread_pool.setMaxThreadCount(num_threads);

  for(int i{0}; i < 365; ++i) {
    ThreadPoolTask* task = new ThreadPoolTask();
    thread_pool.start(task);
  }
  thread_pool.waitForDone(); ///< Wait for all tasks to finish before exiting
  days_processed = 0; ///< If we want to use this mood again, we must reset the critical data.

  time_thread_pool = timer.nsecsElapsed() - time_thread_pool; ///< We stop the chronometer
  std::cout << "------------------------------------------------------------------------------" << std::endl;
  std::cout << "Done in Producer and Consumer mode" << std::endl;
  std::cout << "Number of threads used: " << num_threads << std::endl << std::endl;
}








void DivideAndConquerorMode(const unsigned int num_sub_divisions) {
  std::cout << "Divide and conqueror mode start:" << std::endl;
  std::cout << "------------------------------------------------------------------------------" << std::endl;
  time_divide_and_conqueror = timer.nsecsElapsed(); ///< We run the chronometer



  // days_processed = 0; ///< If we want to use this mood again, we must reset the critical data.
  time_divide_and_conqueror = timer.nsecsElapsed() - time_divide_and_conqueror; ///< We stop the chronometer
  std::cout << "------------------------------------------------------------------------------" << std::endl;
  std::cout << "Done in Producer and Consumer mode" << std::endl;
  std::cout << "Number of threads used: " << std::endl << std::endl;
}








int main(int argc, char* argv[]) {
  QCoreApplication a(argc, argv);

  try {
    ///Process command line parameters -> help
    struct option long_options[] = { ///< argument for an option is stored in optarg (if it was optional_argument, and was not specified, will be a nullptr)
      {"help", no_argument, NULL, 'h'}, ///< option 0 -> help / no arg / no flag / char 'h' which identifies the option (and is also the equivalent short option for convenience)
      {"pool-of-threads", required_argument, NULL, 'p'}, ///< option 1 -> ThreadPool / arg (value) / no flag / char 'p' which identifies the option (and is also the equivalent short option for convenience)
      {"divide-and-conquer", required_argument, NULL, 'd'}, ///< option 2 -> D&C / arg (value) / no flag / char 'd' which identifies the option (and is also the equivalent short option for convenience)
      {"number-of-exec", required_argument, NULL, 'n'}, ///< option 3 -> number of executions / arg (value) / no flag / char 'n' which identifies the option (and is also the equivalent short option for convenience)
      {0, 0, 0, 0} ///< Explicit array termination
    };
    unsigned int number_of_executions{1};
    unsigned int value{0}; ///< Value is for ThreadPool and D&Q versions (thread subdivision and list divisions, respectively)
    char selected_mode = ' ';

    while (true) { ///< if there were arguments after the options, they would need to be processed from optind which indicates index of next element in argv[]
      int option_index = 0;
      char opt_identifier = getopt_long(argc, argv, "hp:d:n:", long_options, &option_index); ///< after an option means required argument

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

        default:
          std::cerr << "Unrecognized command-line parameter..." << std::endl;
          return INVALID_INVOCATION; ///getoptlong already shows an error
      }
    }

    if (number_of_executions < 1 || value < 0) return INVALID_INVOCATION;

    for (int i = 0; i < number_of_executions; ++i) {
      switch (selected_mode) {
        case 'p':
          ThreadPoolMode(value);
          acc_num_measurements[1].first += double(time_thread_pool) / 1000000000; ///< We get the all measurements in a variable in seconds
          ++acc_num_measurements[1].second; ///< We store the times to this mode has been used
          break;

        case 'd':
          DivideAndConquerorMode(value);  ///< We execute the selected mode going to the concret function
          acc_num_measurements[2].first += double(time_divide_and_conqueror) / 1000000000; ///< We get the all measurements in a variable in seconds
          ++acc_num_measurements[2].second; ///< We store the times to this mode has been used
          break;

        default:
          ThreadPoolMode(value);
          //DivideAndConquerorMode(value);
          //SerialMode(); ///< We execute the selected mode going to the concret function
          acc_num_measurements[0].first += double(time_serial) / 1000000000; ///< We get the all measurements in a variable in seconds
          ++acc_num_measurements[0].second; ///< We store the times to this mode has been used
          break;
      };
    }

  } catch (std::exception& error) {
    std::cerr << PROG_NAME << " > EXCEPTION > " << error.what() << std::endl;
    return INVALID_ARG;
  }

  /// We show the relevant data of serial mode
  std::cout << "Last Time Serial: " << (double(time_serial) / 1000000000) << " seconds - Average: "
            << ((acc_num_measurements[0].second != 0) ? acc_num_measurements[0].first / acc_num_measurements[0].second : 0.0)
            << ", Times Executed: " << acc_num_measurements[0].second << std::endl;
  /// We show the relevant data of thread pool mode
  std::cout << "Last Time T.Pool: " << (double(time_thread_pool) / 1000000000) << " seconds - Average: "
            << ((acc_num_measurements[1].second != 0) ? acc_num_measurements[1].first / acc_num_measurements[1].second : 0.0)
            << ", Times Executed: " << acc_num_measurements[1].second  << std::endl;
  /// We show the relevant data of divde and conqueror mode
  std::cout << "Last Time D&C:    " << (double(time_divide_and_conqueror) / 1000000000) << " seconds - Average: "
            << ((acc_num_measurements[2].second != 0) ? acc_num_measurements[2].first / acc_num_measurements[2].second : 0.0)
            << ", Times Executed: " << acc_num_measurements[2].second  << std::endl << std::endl;

  return 0;
}
