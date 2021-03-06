/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (c) 2018 Intel Corporation
 *
 * Authors: Fengguang Wu <fengguang.wu@intel.com>
 *          Yao Yuan <yuan.yao@intel.com>
 */

#include <numa.h>
#include <stdio.h>
#include <sys/user.h>


#include <string>
#include <vector>
#include "lib/stats.h"
#include "ProcVmstat.h"

#include "Numa.h"

int ProcVmstat::load_vmstat()
{
  proc_vmstat = __load_vmstat("/proc/vmstat");
  return proc_vmstat.empty();
}

int ProcVmstat::load_numa_vmstat()
{
  char path[50];
  int rc = 0;

  int max_node = numa_max_node();

  numa_vmstat.clear();
  numa_vmstat.resize(max_node + 1);

  for (int i = 0; i <= max_node; ++i) {
    snprintf(path, sizeof(path), "/sys/devices/system/node/node%d/vmstat", i);
    numa_vmstat[i] = __load_vmstat(path);
    if (numa_vmstat[i].empty())
      ++rc;
  }

  return rc;
}

vmstat_map ProcVmstat::__load_vmstat(const char *path)
{
  vmstat_map map;
  char line[80];
  char key[50];
  unsigned long val;

  FILE *f = fopen(path, "r");
  if (!f) {
    perror(path);
    goto out;
  }

  while (fgets(line, sizeof(line), f))
  {
    int ret = sscanf(line, "%49s %lu\n", key, &val);
    if (ret < 2) {
      if (line[0] != ' ') // ignore known kernel bug
        fprintf(stderr, "parse failed: %d %s\n%s", ret, path, line);
      continue;
    }
    map[key] = val;
  }

  fclose(f);
out:
  return map;
}

unsigned long ProcVmstat::vmstat(std::string name)
{
  if (proc_vmstat.empty())
    load_vmstat();

  return proc_vmstat.at(name);
}

unsigned long ProcVmstat::vmstat(int nid, std::string name)
{
  if (proc_vmstat.empty())
    load_numa_vmstat();

  return numa_vmstat.at(nid).at(name);
}

unsigned long ProcVmstat::anon_capacity()
{
  unsigned long sum = vmstat("nr_free_pages");

  sum += vmstat("nr_inactive_anon");
  sum += vmstat("nr_active_anon");

  return sum;
}

unsigned long ProcVmstat::anon_capacity(int nid)
{
  unsigned long sum = vmstat(nid, "nr_free_pages");

  sum += vmstat(nid, "nr_inactive_anon");
  sum += vmstat(nid, "nr_active_anon");

  return sum;
}

unsigned long ProcVmstat::vmstat(std::vector<int>& nid)
{
  unsigned long sum = 0;

  for (auto& i : nid)
    sum += anon_capacity(i);

  return sum;
}

void ProcVmstat::show_numa_stats(NumaNodeCollection* numa_collection)
{
  unsigned long pmem_anon_kb = 0;
  unsigned long dram_anon_kb = 0;
  NumaNode* numa_obj;

  load_vmstat();
  load_numa_vmstat();

  const auto& numa_vmstat = get_numa_vmstat();
  unsigned long total_anon_kb = vmstat("nr_inactive_anon") +
                                vmstat("nr_active_anon") +
                                vmstat("nr_isolated_anon");

  total_anon_kb *= PAGE_SIZE >> 10;
  printf("\nAnonymous page distribution across NUMA nodes:\n");
  printf("%'15lu       anon total\n", total_anon_kb);

  int nid = 0;
  for (auto& map: numa_vmstat) {
    unsigned long anon_kb = map.at("nr_inactive_anon") +
                            map.at("nr_active_anon") +
                            map.at("nr_isolated_anon");
    anon_kb *= PAGE_SIZE >> 10;
    printf("%'15lu  %2d%%  anon node %d\n", anon_kb, percent(anon_kb, total_anon_kb), nid);

    if (!numa_collection) {
      ++nid;
      continue;
    }
    numa_obj = numa_collection->get_node(nid);
    if (!numa_obj) {
      ++nid;
      continue;
    }

    switch (numa_obj->type()) {
      case NUMA_NODE_DRAM:
        dram_anon_kb += anon_kb;
        break;
      case NUMA_NODE_PMEM:
        pmem_anon_kb += anon_kb;
        break;
      default:
        //for unknown type do nothing
        break;
    }

    ++nid;
  }

  if (numa_collection) {
    printf("Anon DRAM nodes size: %'15lu  %2d%%\n",
           dram_anon_kb, percent(dram_anon_kb, total_anon_kb));
    printf("Anon PMEM nodes size: %'15lu  %2d%%\n",
           pmem_anon_kb, percent(pmem_anon_kb, total_anon_kb));
  }
}
