#include <cassert>
#include <cfloat>
#include <cmath>

#include <algorithm>
#include <exception>
#include <iostream>
#include <fstream>
#include <functional>
#include <random>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>

#include "sim.hh"
#include "test.hh"

using namespace sim;

namespace sim {
  std::mt19937 rng;
}

//////////////////////////////
/* COMMAND LINE PROCESSING */

/*
  Simple command line processing functions taken from:
  http://stackoverflow.com/questions/865668/parse-command-line-arguments
*/

char* getCmdOption(char ** begin, char ** end, const std::string & option)
{
  char ** itr = std::find(begin, end, option);
  if (itr != end && ++itr != end)
    {
      return *itr;
    }
  return 0;
}

bool cmdOptionExists(char** begin, char** end, const std::string& option)
{
  return std::find(begin, end, option) != end;
}

/* END COMMAND LINE PROCESSING */
/////////////////////////////////


bool
sim::is_event(const double risk)
{
  std::uniform_real_distribution<double> uni;
  if (uni(rng) < risk)
    return true;
  else
    return false;
}

void
sim::advanceTimeEvent(Simulation &simulation)
{
  simulation.current_date +=  simulation.time_step;
}

void
sim::deathEvent(Simulation &simulation) {}


std::vector< std::vector<std::string> >
get_csv_table(const char *parameter_file_name)
{
  std::vector< std::vector<std::string> > csv_table;
  std::ifstream infile;

  std::string line, cell;

  infile.open(parameter_file_name);
  if ( infile.is_open() ) {
    while ( std::getline(infile, line) ) {
      std::istringstream line_stream(line);
      std::vector<std::string> cells;
      while ( std::getline(line_stream, cell, ',') )
	if ( cell.size() && cell[0] != '#' )
	  cells.push_back(cell);
	else
      	  break;
      csv_table.push_back(cells);
    }
    infile.close();
  } else {
    std::cout << "Could not open file "
	      << parameter_file_name << "." << std::endl;
  }
  return csv_table;
}

void
process_parameter_file(Common& common, const char * parameter_file_name)
{
  std::vector< std::vector<std::string> > csv_table =
    get_csv_table(parameter_file_name);

  for (auto line : csv_table) {
    std::vector<double> values;
    if ( line.size() > 1 ) {
      for (auto it = line.begin() + 1; it != line.end(); ++it) {
	if ((*it)[0] == '!') {
	  // 01234567
	  // !1.0!0!2
	  double time_period = 1.0;
	  size_t from = 0, step = 1;
	  size_t index1 = (*it).find('!', 1), index2 = std::string::npos;
	  if (index1 != std::string::npos) {
	    time_period = std::stod((*it).substr(1, index1 - 1));
	    index2 = (*it).find('!', index1 + 1);
	    if (index2 != std::string::npos){
	      from = std::stoul((*it).substr(index1 + 1, index2 - index1));
	      step = std::stoul((*it).substr(index2 + 1));
	    } else {
	      from = std::stod((*it).substr(index1 + 1));
	    }
	  } else {
	    time_period = std::stod((*it).substr(1));
	  }
	  common.set_time_adjust(line[0], time_period, from, step);
	} else {
	  values.push_back(std::stod(*it));
	}
      }
      common.set(line[0], values);
    }
  }
}

void
process_command_line_parameters(Common& common, const char *parameter_str)
{
  std::vector<std::string> parm_line;
  std::string s(parameter_str);
  size_t i = 0, j = 0;

  do {
    j = s.find(":", i);
    if (j == std::string::npos)
      j = s.size();
    std::string p = s.substr(i, j - i);
    i = j + 1;
    parm_line.push_back(p);
  } while(j != s.size());

  for (auto p : parm_line) {
    if ( (p[0] >= 'a' && p[0] <= 'z') ||
	 (p[0] >= 'A' && p[0] <= 'Z')) {
      s = p;
      common.set(s, {});
    } else if (p[0] == '!') {
      common.set_time_adjust(s, stod(p.substr(1)));
    } else {
      common.push_back(s, stod(p));
    }
  }
}

void run_tests()
{
  Common c;
  Simulation s(c);
  tst::TestSeries t;

  process_parameter_file(s.common, "test_csv.csv");
  c.set_defaults_not_yet_set();

  TESTDBL(t, s.common("NEW_PARM", 0), 5.0, "parameter type added");
  TESTDBL(t, s.common("NEW_PARM", 1), 1.0, "multiple entries added");
  TESTDBL(t, s.common("HIV_PREVALENCE_STAGE",0), 0.2, "entry correct 1");
  TESTDBL(t, s.common("HIV_PREVALENCE_STAGE",1), 0.1, "entry correct 2");
  TESTDBL(t, s.common("HIV_PREVALENCE_STAGE",2), 0.3, "entry correct 3");
  TESTDBL(t, s.common("HIV_PREVALENCE_STAGE",3), 0.2, "entry correct 4");
  TESTDBL(t, s.common.get_time_adjust("HIV_PREVALENCE_STAGE").time_period, 1.0,
	  "time adjuster set");
  TESTDBL(t, s.common.get_time_adjust("HIV_PREVALENCE_STAGE").from, 0,
	  "from set");
  TESTDBL(t, s.common.get_time_adjust("HIV_PREVALENCE_STAGE").step, 1,
	  "step set");
  TESTDBL(t, s.common.get_time_adjust("HIV_RISK_DEATH").time_period, 1.25,
	  "time adjuster set");
  TESTDBL(t, s.common.get_time_adjust("HIV_RISK_DEATH").from, 1,
	  "from set");
  TESTDBL(t, s.common.get_time_adjust("HIV_RISK_DEATH").step, 2,
	  "step set");
  s.common.adjust_parameters_to_time_period();
  TESTDBL(t, s.common("HIV_PREVALENCE_STAGE", 1),
	  0.00202411247526512738659221213311, "time corrected probability");
  TESTDBL(t, s.common("HIV_RISK_DEATH", 0), 500,
	  "index into time adjuster left alone 1");
  TESTDBL(t, s.common("HIV_RISK_DEATH", 2), 400,
	  "index into time adjuster left alone 2");
  TESTDBL(t, s.common("HIV_RISK_DEATH", 4), 350,
	  "index into time adjuster left alone 3");
  TESTDBL(t, s.common("HIV_RISK_DEATH", 6), 200,
	  "index into time adjuster left alone 4");
  TESTDBL(t, s.common("HIV_RISK_DEATH", 8), 50,
	  "index into time adjuster left alone 4");
  TESTDBL(t, s.common("HIV_RISK_DEATH", 9), 0.0139978861008127619669494379195,
	  "correct time adjustment");

  try {
    s.common("HIV_PREVALENCE_STAGE",4);
    TESTEQ(t, 0, 1, "exception not thrown 1");
  } catch (InvalidParameter &e) {
    TESTEQ(t, 1, 1, "exception thrown 1");
  }
  try {
    s.common("HIV_PREVALENCE_");
    TESTEQ(t, 0, 1, "exception not thrown 2");
  } catch(InvalidParameter &e) {
    TESTEQ(t, 1, 1, "exception thrown 2");
  }

  size_t n = s.common("NUM_AGENTS");
  s.init({advanceTimeEvent, deathEvent});
  TESTEQ(t, s.agents.size(), n, "number of agents before simulation");
  s.simulate();
  TESTEQ(t, s.agents.size(), n, "number of agents after simulation");
  TESTDBL(t, s.current_date,
	  (s.common("START_DATE") +
	   (s.common("ITERATIONS") * s.common("TIME_STEP"))),
	  "end and start dates correspond to time steps");
  t.summary();
}


void sim::process_command_line(Common &common, int argc, char *argv[])
{
  // Command line options
  bool test = cmdOptionExists(argv, argv + argc, "-t");
  bool verbose = cmdOptionExists(argv, argv + argc, "-v");
  char *parameter_file_str = getCmdOption(argv, argv + argc, "-f");
  char *verbosity_str = getCmdOption(argv, argv + argc, "-v");
  char *parameter_str = getCmdOption(argv, argv + argc, "-p");

  if (test)
    run_tests();

  if (parameter_file_str)
    process_parameter_file(common, parameter_file_str);

  if (parameter_str)
    process_command_line_parameters(common, parameter_str);

  // Determine whether or not to print the parameters
  bool print_parms = false;
  if (verbose) {
    if (verbosity_str) {
      std::string s(verbosity_str);
      if (s.find('p') != std::string::npos) {
	print_parms = true;
      };
    } else {
      print_parms = true;
    }
    if (print_parms) {
      std::cout << "************************" << std::endl;
      std::cout << "Parameters" << std::endl;
      common.print_parameters();
      std::cout << "************************" << std::endl;
    }
  }
}

void sim::default_agent_initiation(Agent *a, Common &c)
{
  std::uniform_real_distribution<double> uni;
  a->id = c.last_agent++;
  a->alive = true;
  a->sex = uni(rng) < c("PROB_MALE") ?
		   MALE : FEMALE;
  { // dob
    std::uniform_real_distribution<double>
      uni_age(c("EARLIEST_BIRTH_DATE"),
	      c("LATEST_BIRTH_DATE"));
    a->dob = uni_age(rng);
  }
  a->cd4 = 1000;
  a->hiv = 0;
  for (int i = 0; i < 4; ++i) {
    if (uni(rng) < c("HIV_PREVALENCE_STAGE", i)) {
      a->hiv = i;
      break;
    }
  }
  a->riskiness = uni(rng);
  a->orientation = 1.0;
  a->num_partners = 0;
  if (uni(rng) < c("PROB_CIRCUMCISED"))
    a->circumcised = true;

}


double
sim::time_correct_prob(const double parameter_prob,
		       const double parameter_time_period,
		       const double actual_time_period)
{

  // 1-(1-p)^t;
  double t = actual_time_period / parameter_time_period;
  double not_p = 1 - parameter_prob;
  double not_p_corrected = pow(not_p, t);
  double new_p = 1 - not_p_corrected;
  return new_p;
}


double sim::Agent::age(Simulation &s)
{
  return s.current_date - dob;
}

void sim::Agent::die(Simulation &s, const std::string & c)
{
  alive = false;
  dod = s.current_date;
  cause = c;
}