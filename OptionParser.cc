#include <errno.h>
#include "OptionParser.h"


OptionParser::OptionParser()
  : Option()
{
}

OptionParser::~OptionParser()
{
}

int OptionParser::parse_file(std::string filename)
{
  config_file = filename;
  return reparse();
}

int OptionParser::reparse()
{
  int ret_val;
  YAML::Node config;

  try {
    config = YAML::LoadFile(config_file);
  } catch (...) {
    printf("ERROR: exception on loading YAML file %s\n", config_file.c_str());
    return -1;
  }

  try {

    ret_val = parse_option(config["options"]);
    if (ret_val < 0) {
      printf("ERROR: failed to parse options\n");
      return ret_val;
    }

    ret_val = parse_policies(config["policies"]);

    if (dump_options)
      dump();

  } catch (...) {
    ret_val = -1;
    printf("ERROR: exception on parsing options/policies\n");
  }

  return ret_val;
}

template<typename Tval>
int OptionParser::get_value(const YAML::const_iterator  &iter,
                            const char* key_name, Tval &value)
{
  std::string key = iter->first.as<std::string>();
  if (!key.compare(key_name))
  {
    value = iter->second.as<Tval>();
    return 1;
  }

  return 0;
}

int OptionParser::parse_option(YAML::Node &&option_node)
{
    if (!option_node)
      return -1;

    if (!option_node.IsMap())
      return -1;

    for (YAML::const_iterator iter = option_node.begin();
         iter != option_node.end();
         ++iter) {
#define OP_GET_VALUE(name, member) if (get_value(iter, name, member)) continue;
      OP_GET_VALUE("interval",        interval);
      OP_GET_VALUE("initial_interval", initial_interval);
      OP_GET_VALUE("sleep",           sleep_secs);
      OP_GET_VALUE("loop",            nr_loops);
      OP_GET_VALUE("max_threads",     max_threads);
      OP_GET_VALUE("split_rss_size",  split_rss_size);
      OP_GET_VALUE("bandwidth_mbps",  bandwidth_mbps);
      OP_GET_VALUE("dram_percent",    dram_percent);
      OP_GET_VALUE("output",          output_file);
      OP_GET_VALUE("exit_on_stabilized", exit_on_stabilized);
      OP_GET_VALUE("numa_dram_nodes", numa_hw_config.numa_dram_list);
      OP_GET_VALUE("numa_pmem_nodes", numa_hw_config.numa_pmem_list);
      OP_GET_VALUE("numa_peer_nodes", numa_hw_config.pmem_dram_map);
#undef OP_GET_VALUE

      std::string str_val;
      if (get_value(iter, "dump_options", str_val)) {
        Option::parse_name_map(bool_name_map, str_val, dump_options, 1);
        continue;
      }
      if (get_value(iter, "exit_on_exceeded", str_val)) {
        Option::parse_name_map(bool_name_map, str_val, exit_on_exceeded, 1);
        continue;
      }

      // parse_common_policy(iter, default_policy);
    }

    return 0;
}

int OptionParser::parse_policies(YAML::Node &&policies_node)
{
    if (!policies_node)
      return -1;

    if (!policies_node.IsSequence())
      return -1;

    for (std::size_t i = 0; i < policies_node.size(); ++i) {
      if (!policies_node[i].IsMap())
        continue;

      parse_one_policy(policies_node[i]);
    }

    return 0;
}

void OptionParser::parse_common_policy(const YAML::const_iterator& iter, Policy& policy)
{
  std::string str_val;

  if (get_value(iter, "migration", str_val)) {
    policy.migrate_what
      = Option::parse_migrate_name(str_val);
    return;
  }

  if (get_value(iter, "placement", str_val)) {
    Option::parse_name_map(placement_name_map, str_val, policy.placement, PLACEMENT_END);
    return;
  }

  if (get_value(iter, "dump_distribution", str_val)) {
    Option::parse_name_map(bool_name_map, str_val, policy.dump_distribution, 2);
    return;
  }
}

void OptionParser::parse_one_policy(YAML::Node &&policy_node)
{
    struct Policy new_policy;

    for (auto iter = policy_node.begin();
         iter != policy_node.end();
         ++iter) {
      if (get_value(iter, "pid", new_policy.pid))
        continue;
      if (get_value(iter, "name", new_policy.name))
        continue;

      parse_common_policy(iter, new_policy);
    }

    add_policy(new_policy);
}
