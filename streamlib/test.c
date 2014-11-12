#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <streams.h>

#define RUNTEST(x) { printf("Running " #x "...\n"); if (x() != 0) { printf("Test failed!\n"); return -1; } }
#define ASSERT(x) if (!(x)) { printf("Assert failed: " #x " on line %d\n", __LINE__); return -1; }
#define ASSERT_EQUALS(x, y) { int z = (x); if (z != y) { printf("Assert failed: Expected " #x " = %d to equal " #y " on line %d\n", z, __LINE__); return -1; } }
#define ASSERT_NOTEQUALS(x, y) { int z = (x); if (z == y) { printf("Assert failed: Expected " #x " = %d to NOT equal " #y " on line %d\n", z, __LINE__); return -1; } }
#define ASSERT_NOERROR(x) { int z = (x); if (z < 0) { printf("Assert failed: Expected " #x " = %d to be >= 0 on line %d\n", z, __LINE__); return -1; } }
#define ASSERT_ERROR(x) { int z = (x); if (z >= 0) { printf("Assert failed: Expected " #x " = %d to be < 0 on line %d\n", z, __LINE__); return -1; } }

// Some stream pointers we can use in all our tests
TKStreamPointer str_in;
TKStreamPointer str_out;

// A few interesting structures we can read and write
#define TKS_ID_TEST_A 1001
typedef struct {
    TKMsgHeader header; // TKS_ID_TEST_A
    unsigned int number;
} TKMsgTestA;

#define TKS_ID_TEST_B 1002
typedef struct {
    TKMsgHeader header; // TKS_ID_TEST_B
    unsigned char short_str[16];
} TKMsgTestB;

int test_init_streams() {
    // Initialize the streams
    char buf[16];
    streamInitStreams(&str_in, &str_out, buf, buf, 16);

    // Check the everything is initialized correctly
    ASSERT_EQUALS(str_in.flags & TKS_FLAGS_WRITE, 0);
    ASSERT_NOTEQUALS(str_out.flags & TKS_FLAGS_WRITE, 0);
    ASSERT_EQUALS(str_in.buffer_size, 16);
    ASSERT_EQUALS(str_out.buffer_size, 16);
    ASSERT(str_in.buffer_ptr == buf);
    ASSERT(str_out.buffer_ptr == buf);
    ASSERT_EQUALS(str_in.read_offset, 0);
    ASSERT_EQUALS(str_out.read_offset, 0);
    ASSERT_EQUALS(str_in.write_offset, 0);
    ASSERT_EQUALS(str_out.write_offset, 0);
    ASSERT_EQUALS(str_in.cur_offset, 0);
    ASSERT_EQUALS(str_out.cur_offset, 0);

    // Syncing now should succeed
    ASSERT_NOERROR(streamSyncStreams(&str_in, &str_out));

    // Syncing does nothing
    ASSERT_EQUALS(str_in.read_offset, 0);
    ASSERT_EQUALS(str_out.read_offset, 0);
    ASSERT_EQUALS(str_in.write_offset, 0);
    ASSERT_EQUALS(str_out.write_offset, 0);
    ASSERT_EQUALS(str_in.cur_offset, 0);
    ASSERT_EQUALS(str_out.cur_offset, 0);

    return 0;
}

int test_normal_read_write() {
    // Initialize the streams
    char buf[4096];
    TKMsgTestA *a_msg;
    TKMsgTestB *b_msg;
    streamInitStreams(&str_in, &str_out, buf, buf, 2048);

    // Can't read until we've done a write
    ASSERT(streamReadMsg(&str_in) == 0);

    // Write an A message
    ASSERT((a_msg = (TKMsgTestA*)streamCreateMsg(&str_out, TKS_ID_TEST_A, sizeof(TKMsgTestA))) != 0);
    a_msg->number = 12345;

    // Write a B message
    ASSERT((b_msg = (TKMsgTestB*)streamCreateMsg(&str_out, TKS_ID_TEST_B, sizeof(TKMsgTestB))) != 0);
    strcpy(b_msg->short_str, "Test");

    // cur_offset is updated, but write_offset is not
    ASSERT_EQUALS(str_out.cur_offset, sizeof(TKMsgTestA) + sizeof(TKMsgTestB));
    ASSERT_EQUALS(str_in.write_offset, 0);
    ASSERT_EQUALS(str_out.write_offset, 0);

    // Sync!
    ASSERT_NOERROR(streamSyncStreams(&str_in, &str_out));

    // Now both streams have the updated write_offset
    ASSERT_EQUALS(str_in.write_offset, sizeof(TKMsgTestA) + sizeof(TKMsgTestB));
    ASSERT_EQUALS(str_out.write_offset, sizeof(TKMsgTestA) + sizeof(TKMsgTestB));

    // Read back A message
    ASSERT((a_msg = (TKMsgTestA*)streamReadMsg(&str_in)) != 0);
    ASSERT_EQUALS(a_msg->header.identifier, TKS_ID_TEST_A);
    ASSERT_EQUALS(a_msg->header.length, sizeof(TKMsgTestA));
    ASSERT_EQUALS(a_msg->number, 12345);

    // Read back B message
    ASSERT((b_msg = (TKMsgTestB*)streamReadMsg(&str_in)) != 0);
    ASSERT_EQUALS(b_msg->header.identifier, TKS_ID_TEST_B);
    ASSERT_EQUALS(b_msg->header.length, sizeof(TKMsgTestB));
    ASSERT(strcmp(b_msg->short_str, "Test") == 0);

    // cur_offset is updated, but read_offset is not
    ASSERT_EQUALS(str_in.cur_offset, sizeof(TKMsgTestA) + sizeof(TKMsgTestB));
    ASSERT_EQUALS(str_in.read_offset, 0);
    ASSERT_EQUALS(str_out.read_offset, 0);

    // Can't read any more
    ASSERT(streamReadMsg(&str_in) == 0);

    // Sync!
    ASSERT_NOERROR(streamSyncStreams(&str_in, &str_out));

    // Now both streams have the updated read_offset
    ASSERT_EQUALS(str_in.read_offset, sizeof(TKMsgTestA) + sizeof(TKMsgTestB));
    ASSERT_EQUALS(str_out.read_offset, sizeof(TKMsgTestA) + sizeof(TKMsgTestB));

    return 0;
}

int test_write_too_big() {
    char buf1[64];
    char buf2[10000];

    // Can't write a message bigger than the stream
    streamInitStreams(&str_in, &str_out, buf1, buf1, 64);
    ASSERT(streamCreateMsg(&str_out, TKS_ID_TEST_A, 65) == 0);

    // Can't write a message the size of the stream (cannot cause the read
    // and write heads to overlap on a write; only on a read)
    ASSERT(streamCreateMsg(&str_out, TKS_ID_TEST_A, 64) == 0);

    // Can't write a message smaller than the message header
    ASSERT(streamCreateMsg(&str_out, TKS_ID_TEST_A, 1) == 0);

    // Can write the size of the stream - 1
    ASSERT(streamCreateMsg(&str_out, TKS_ID_TEST_A, 63) != 0);

    // Now the stream is full; can no longer write to it, not even one header
    ASSERT(streamCreateMsg(&str_out, TKS_ID_TEST_A, sizeof(TKMsgHeader)) == 0);

    // Can't write a message bigger than TKS_MAX_MESSAGE_SIZE
    streamInitStreams(&str_in, &str_out, buf1, buf1, 10000);
    ASSERT(streamCreateMsg(&str_out, TKS_ID_TEST_A, TKS_MAX_MESSAGE_SIZE+1) == 0);
}

int test_flags() {
    char buf1[256];
    TKMsgTestA *a_msg;
    streamInitStreams(&str_in, &str_out, buf1, buf1, 256);

    // Write an A message normally
    ASSERT((a_msg = (TKMsgTestA*)streamCreateMsg(&str_out, TKS_ID_TEST_A, sizeof(TKMsgTestA))) != 0);
    a_msg->number = 12345;

    // Can't read from the write stream
    ASSERT(streamCreateMsg(&str_in, TKS_ID_TEST_A, sizeof(TKMsgTestA)) == 0);
    ASSERT(streamReadMsg(&str_out) == 0);
}

int test_overrun() {
    // Message A is 12 bytes
    // Message B is 24 bytes
    // Make a stream just big enough to hold A + B - 4 bytes. The buffer
    // needs to be double that to handle possible overrun.
    char buf[64];
    TKMsgTestA *a_msg;
    TKMsgTestB *b_msg;
    streamInitStreams(&str_in, &str_out, buf, buf, 32);

    // Write an A message
    ASSERT((a_msg = (TKMsgTestA*)streamCreateMsg(&str_out, TKS_ID_TEST_A, sizeof(TKMsgTestA))) != 0);
    a_msg->number = 12345;

    // Sync, read the A message, and sync again to free up the first 12 bytes
    ASSERT_NOERROR(streamSyncStreams(&str_in, &str_out));
    ASSERT((a_msg = (TKMsgTestA*)streamReadMsg(&str_in)) != 0);
    ASSERT_NOERROR(streamSyncStreams(&str_in, &str_out));

    // Write a B message (this will write 4 bytes past the end of the stream)
    ASSERT((b_msg = (TKMsgTestB*)streamCreateMsg(&str_out, TKS_ID_TEST_B, sizeof(TKMsgTestB))) != 0);
    strcpy(b_msg->short_str, "ABCDEFGHIJKLMNO");

    ASSERT_EQUALS(str_out.cur_offset, 4);
    // However, because the buffer is contiguous it actually wrote the bytes
    // "past the end" of the stream buffer and not at the beginning, which
    // still has the A message identifier
    ASSERT(buf[32] == 'M');
    ASSERT(buf[33] == 'N');
    ASSERT(buf[34] == 'O');
    ASSERT_EQUALS(((unsigned int*)buf)[0], TKS_ID_TEST_A);

    // Sync, read the B message, and sync again to free up all the bytes
    ASSERT_NOERROR(streamSyncStreams(&str_in, &str_out));
    ASSERT((b_msg = (TKMsgTestA*)streamReadMsg(&str_in)) != 0);
    ASSERT_NOERROR(streamSyncStreams(&str_in, &str_out));

    // Write another A message, make sure it gets written to the right place
    ASSERT((a_msg = (TKMsgTestA*)streamCreateMsg(&str_out, TKS_ID_TEST_A, sizeof(TKMsgTestA))) != 0);
    a_msg->number = 5678;
    ASSERT_EQUALS(((unsigned int*)buf)[1], TKS_ID_TEST_A);
    ASSERT_EQUALS(((unsigned int*)buf)[3], 5678);

    return 0;
}

int main() {
    RUNTEST(test_init_streams);
    RUNTEST(test_normal_read_write);
    RUNTEST(test_write_too_big);
    RUNTEST(test_flags);
    RUNTEST(test_overrun);
    printf("All tests pass. Yay!\n");
    return 0;
}
