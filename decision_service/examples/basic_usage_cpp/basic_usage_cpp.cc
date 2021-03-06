// Reinforcement learning API demo
#include "basic_usage_cpp.h"

int main() {
  // name, value based config object used to initialise the API
  u::config_collection config;

  // Helper method to initialize config from a json file
  if( load_config_from_json("client.json", config) != err::success ) {
    std::cout << "Unable to Load file: client.json" << std::endl;
    return -1;
  }

  // api_status is an optional argument used to get detailed 
  // error description from all API calls
  r::api_status status;

  // (1) Instantiate Inference API using config
  r::live_model rl(config);

  // (2) Initialize the API
  if( rl.init(&status) != err::success ) {
    std::cout << status.get_error_msg() << std::endl;
    return -1;
  }

  // Response class
  r::ranking_response response;

  // (3) Choose an action
  if( rl.choose_rank(uuid, context, response, &status) != err::success ) {
    std::cout << status.get_error_msg() << std::endl;
    return -1;
  }

  // (4) Use the response
  size_t choosen_action;
  if( response.get_choosen_action_id(choosen_action, &status) != err::success ) {
    std::cout << status.get_error_msg() << std::endl;
    return -1;
  }
  std::cout << "Chosen action id is: " << choosen_action << std::endl;

  // (5) Report recieved reward (Optional: if this call is not made, default missing reward is applied)
  //     Missing reward can be thought of as negative reinforcement
  if( rl.report_outcome(uuid, reward, &status) != err::success ) {
    std::cout << status.get_error_msg() << std::endl;
    return -1;
  }

  return 0;
}

// Helper methods

// Load config from json file
int load_config_from_json(const std::string& file_name, u::config_collection& cfgcoll) {
  std::string config_str;
  // Load contents of config file into a string
  const auto scode = load_file(file_name, config_str);
  if ( scode != 0 ) return scode;
  // Use library supplied convinence method to parse json and build config object
  return cfg::create_from_json(config_str, cfgcoll);
}

// Load contents of file into a string
int load_file(const std::string& file_name, std::string& config_str) {
  std::ifstream fs;
  fs.open(file_name);
  if ( !fs.good() )
    return reinforcement_learning::error_code::invalid_argument;
  std::stringstream buffer;
  buffer << fs.rdbuf();
  config_str = buffer.str();
  return reinforcement_learning::error_code::success;
}
