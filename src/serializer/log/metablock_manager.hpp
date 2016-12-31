// Copyright 2010-2012 RethinkDB, all rights reserved.
#ifndef SERIALIZER_LOG_METABLOCK_MANAGER_HPP_
#define SERIALIZER_LOG_METABLOCK_MANAGER_HPP_

#include <stddef.h>
#include <string.h>

#include <vector>

#include "arch/compiler.hpp"
#include "arch/types.hpp"
#include "concurrency/mutex.hpp"
#include "serializer/log/extent_manager.hpp"
#include "serializer/log/static_header.hpp"
#include "serializer/log/metablock.hpp"


#define MB_BLOCKS_PER_EXTENT 8

#define MB_BAD_VERSION (-1)
#define MB_START_VERSION 1



/* TODO support multiple concurrent writes */

std::vector<int64_t> initial_metablock_offsets(int64_t extent_size);

class metablock_manager_t {
public:
    explicit metablock_manager_t(extent_manager_t *em);
    ~metablock_manager_t();

    /* Clear metablock slots and write an initial metablock to the database file */
    static void create(file_t *dbfile, int64_t extent_size,
                       log_serializer_metablock_t *initial);

    /* Tries to load existing metablocks */
    void co_start_existing(file_t *dbfile, bool *mb_found,
                           log_serializer_metablock_t *mb_out);
    struct metablock_read_callback_t {
        virtual void on_metablock_read() = 0;
        virtual ~metablock_read_callback_t() {}
    };

    bool start_existing(file_t *dbfile, bool *mb_found,
                        log_serializer_metablock_t *mb_out,
                        metablock_read_callback_t *cb);

    struct metablock_write_callback_t {
        virtual void on_metablock_write() = 0;
        virtual ~metablock_write_callback_t() {}
    };
    void write_metablock(log_serializer_metablock_t *mb,
                         file_account_t *io_account,
                         metablock_write_callback_t *cb);
    void co_write_metablock(log_serializer_metablock_t *mb, file_account_t *io_account);

    void shutdown();

private:
    struct head_t {
    private:
        uint32_t mb_slot;
        uint32_t saved_mb_slot;
    public:
        // whether or not we've wrapped around the edge (used during startup)
        bool wraparound;
    public:
        explicit head_t(metablock_manager_t *mgr);
        metablock_manager_t *const mgr;

        // handles moving along successive mb slots
        void operator++();

        // return the offset we should be writing to
        int64_t offset();

        // save the state to be loaded later (used to save the last known uncorrupted
        // metablock)
        void push();

        //load a previously saved state (stack has max depth one)
        void pop();
    };

    void start_existing_callback(file_t *dbfile,
                                 bool *mb_found,
                                 log_serializer_metablock_t *mb_out,
                                 metablock_read_callback_t *cb);
    void write_metablock_callback(log_serializer_metablock_t *mb,
                                  file_account_t *io_account,
                                  metablock_write_callback_t *cb);

    mutex_t write_lock;

    // keeps track of where we are in the extents
    head_t head;

    metablock_version_t next_version_number;

    const scoped_device_block_aligned_ptr_t<crc_metablock_t> mb_buffer;
    // true: we're using the buffer, no one else can
    bool mb_buffer_in_use;

    extent_manager_t *const extent_manager;

    const std::vector<int64_t> metablock_offsets;

    enum state_t {
        state_unstarted,
        state_reading,
        state_ready,
        state_writing,
        state_shut_down
    } state;

    file_t *dbfile;

    DISABLE_COPYING(metablock_manager_t);
};

#endif /* SERIALIZER_LOG_METABLOCK_MANAGER_HPP_ */
