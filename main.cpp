#include <QCoreApplication>
#include <QVector>
#include <iostream>
#include <fstream>
#include <getopt.h>
#include <math.h>

#define INVALID_ARG 1
#define INVALID_INVOCATION 2
#define HELP_ASKED 3

const char* PROG_NAME = "DC_THREADPOOL";
const char* MANUAL_PATH = "manual.hlp";

void ShowHelp() {
  std::ifstream fs_in (MANUAL_PATH, std::ios_base::in);

  if (fs_in.is_open()) {
    std::string current_line;

    while (std::getline(fs_in, current_line))
      std::cout << current_line << std::endl;

  } else
    std::cerr << "Help file (" << MANUAL_PATH << ") is missing! Program " << PROG_NAME << " is going to be aborted as correct functionality cannot be ensured" << std::endl;
}

class Statistics {
  // https://stackoverflow.com/questions/7988486/how-do-you-calculate-the-variance-median-and-standard-deviation-in-c-or-java/7988556#7988556
  QVector<float> data;
  int size;

 public:
  Statistics(QVector<float> data) {
    this->data = data;
    size = data.length();
  }

  double getMean() {
    double sum = 0.0;

    for(double a : data)
      sum += a;

    return sum / size;
  }

  double median() {
    //Arrays.sort(data);
    std::sort(data.begin(), data.end());

    if (data.length() % 2 == 0)
      return (data[(data.length() / 2) - 1] + data[data.length() / 2]) / 2.0;

    return data[data.length() / 2];
  }
};

void SerialMode() {
  QVector<float> d;
  unsigned int total = 60 * 60 * 24 * 365; // sec * min * hour * days

  for(unsigned int i = 0; i < total; i++) {
    d.push_back(random() % 50 + 50);

    if(i % (24 * 3600) == 0) {
      Statistics s(d);
      s.getMean();
      s.median();
      std::cout << i << " of " << total << " ";
      std::cout << "average temperature " << s.getMean() << " ";
      std::cout << " with median " << s.median() << "\n";
      d.erase(d.begin(), d.end());
    }
  }

  std::cout << "Done in serial mode\n";
}

int main(int argc, char* argv[]) {
  QCoreApplication a(argc, argv);

  try {
    ///Process command line parameters -> help
    struct option long_options[] = { ///argument for an option is stored in optarg (if it was optional_argument, and was not specified, will be a nullptr)
      {"help", no_argument, NULL, 'h'}, ///option 0 -> help / no arg / no flag / char 'h' which identifies the option (and is also the equivalent short option for convenience)
      {"pool-of-threads", required_argument, NULL, 'p'}, ///option 1 -> ThreadPool / arg (value) / no flag / char 'p' which identifies the option (and is also the equivalent short option for convenience)
      {"divide-and-conquer", required_argument, NULL, 'd'}, ///option 2 -> D&C / arg (value) / no flag / char 'd' which identifies the option (and is also the equivalent short option for convenience)
      {"number-of-exec", required_argument, NULL, 'n'}, ///option 3 -> number of executions / arg (value) / no flag / char 'n' which identifies the option (and is also the equivalent short option for convenience)
      {0, 0, 0, 0} ///Explicit array termination
    };
    int number_of_executions = 1, value = 0; /// Value is for ThreadPool and D&Q versions (thread subdivision and list divisions, respectively)
    char selected_mode = ' ';

    while (true) { ///if there were arguments after the options, they would need to be processed from optind which indicates index of next element in argv[]
      int option_index = 0;
      char opt_identifier = getopt_long(argc, argv, "hp:d:n:", long_options, &option_index); ///: after an option means required argument

      if (opt_identifier == -1) break; ///No more options of either size

      switch (opt_identifier) {
        case 'h':
          ShowHelp();
          return HELP_ASKED;

        case 'p': ///ThreadPool
          selected_mode = 'p';
          value = std::stoi(optarg);
          break;

        case 'd': ///Divide & Conquer
          selected_mode = 'd';
          value = std::stoi(optarg);
          break;

        case 'n': ///Several executions
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
          /// Call ThreadPool Function
          break;

        case 'd':
          /// Call D&Q Function
          break;

        default:
          /// Resort to serial mode
          SerialMode();
          break;
      };
    }

  } catch (std::exception& error) {
    std::cerr << PROG_NAME << " > EXCEPTION > " << error.what() << std::endl;
    return INVALID_ARG;
  }

  return 0;
}
