#ifndef _MOVE_PAGES_H
#define _MOVE_PAGES_H

#include <unordered_map>
#include <string>
#include <vector>

#include "ProcIdlePages.h"

class BandwidthLimit;
class Formatter;
class NumaNodeCollection;
class NumaNode;

#define MPOL_MF_SW_YOUNG (1<<7)

typedef std::unordered_map<int, int> MovePagesStatusCount;
class NumaNodeCollection;


struct MoveStats
{
    unsigned long to_move_kb;
    unsigned long skip_kb;
    unsigned long move_kb;

    void clear();
    void account(MovePagesStatusCount& status_count, int page_shift, int target_node);
};

class MovePages
{
  public:
    MovePages();
    ~MovePages() {};

    void set_pid(pid_t i)                     { pid = i; }
    void set_page_shift(int t)                { page_shift = t; }
    void set_batch_size(unsigned long npages) { batch_size = npages; }
    void set_flags(int f)                     { flags = f; }
    void set_throttler(BandwidthLimit* new_throttler) { throttler = new_throttler; }
    void set_migration_type(ProcIdlePageType new_type) { type = new_type; }
    long move_pages(std::vector<void *>& addrs, bool is_locate);
    long move_pages(void **addrs, unsigned long count, bool is_locate);
    long locate_move_pages(std::vector<void *>& addrs,
                           MoveStats *stats);
    void set_numacollection(NumaNodeCollection* new_collection)
    { numa_collection = new_collection; }

    std::vector<int>& get_status()            { return status; }
    MovePagesStatusCount& get_status_count()  { return status_count; }
    void clear_status_count()                 { status_count.clear(); }
    void calc_status_count();
    void add_status_count(MovePagesStatusCount& status_sum);
    void show_status_count(Formatter* fmt);
    void show_status_count(Formatter* fmt, MovePagesStatusCount& status_sum);
    void account_stats(MoveStats *stats);
    void calc_target_nodes(void);
    int get_target_node(NumaNode* node_obj);
    bool is_node_in_target_set(int node_id);

  private:
    pid_t pid;
    int flags;

    unsigned long page_shift;
    unsigned long batch_size; // used by locate_move_pages()

    // Get the status after migration
    std::vector<int> status;

    std::vector<int> target_nodes;

    MovePagesStatusCount status_count;

    BandwidthLimit* throttler;

    ProcIdlePageType type;

    NumaNodeCollection* numa_collection;
};

#endif
// vim:set ts=2 sw=2 et:
