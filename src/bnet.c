#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

struct msg_header{
        uint16_t size;
        uint16_t count;
        uint16_t* ids;
        void ** data;
};

struct msg_computer{
        uint64_t uptime;
        uint64_t update;
        uint16_t distance;
        uint16_t hostname_count;
        char** hostnames;
        char* name;
        char* msg;
};

unsigned int add_data(void * dst, void * src, unsigned int size)
{
        if (dst == NULL || src == NULL || size == 0){
                return 0;
        }
        memcpy(dst, src, size);
        return size;
}

unsigned int get_string(char ** dst, void * src)
{
        unsigned int size = strlen(src) + 1;
        *dst = malloc(sizeof(char) * size);
        memcpy(*dst, src, size);
        return size;
}

unsigned int decode_computer(struct msg_computer* dst, void * src)
{
        if(src == NULL){
                return 0;
        }

        dst = malloc(sizeof(struct msg_computer));
        unsigned int offset = 0;

        offset += add_data(&dst->uptime, src + offset, sizeof(dst->uptime));
        dst->uptime = ntohll(dst->uptime);
        offset += add_data(&dst->update, src + offset, sizeof(dst->update));
        dst->update = ntohll(dst->update);
        offset += add_data(&dst->distance, src + offset, sizeof(dst->distance));
        dst->distance = ntohs(dst->distance);
        offset += add_data(&dst->hostname_count, src + offset, sizeof(dst->hostname_count));
        dst->hostname_count = ntohs(dst->hostname_count);

        //Get number of hostnames (required for calculating size)
        dst->hostnames = malloc(sizeof(char*) * dst->hostname_count);
        for(int i = 0; i < dst->hostname_count; i++){
                offset += get_string(&dst->hostnames[i], src + offset);
        }

        offset += get_string(&dst->name, src + offset);
        offset += get_string(&dst->msg, src + offset);

        return offset;
}


struct msg_header decode_packet(void* data)
{
        struct msg_header mhead;
        if(data == NULL){
                return mhead;
        }

        //Get header info.
        unsigned int offset = 0;
        offset += add_data(&mhead.size, data + offset, sizeof(mhead.size));
        mhead.size = ntohs(mhead.size);
        offset += add_data(&mhead.count, data + offset, sizeof(mhead.count));
        mhead.count = ntohs(mhead.count);

        //If there are no objects, we're done. And the packet was useless.
        if(mhead.count == 0){
                mhead.ids = NULL;
                mhead.data = NULL;
                return mhead;
        }

        //Get ids
        mhead.ids = malloc(sizeof(uint16_t) * mhead.count);
        for(unsigned int i = 0; i < mhead.count; i++){
                offset += add_data(mhead.ids + i, data + offset, sizeof(uint16_t));
                mhead.ids[i] = ntohs(mhead.ids[i]);
        }

        //Get data objects and read into data array
        mhead.data = malloc(sizeof(void*) * mhead.count);
        for(unsigned int i = 0; i < mhead.count; i++){
                switch(mhead.ids[i]){
                case 2:
                        offset += decode_computer(mhead.data[i], data + offset);
                        break;
                default:
                        mhead.data[i] = NULL;
                        break;
                }
        }

        return mhead;
}


int main(int argc, char* argv[])
{
        //vars for building data.
        unsigned int offset = 0;
        uint16_t size = 0;

        //Build one packet for this computer.
        struct msg_header mhead;
        mhead.count = htons(1);
        mhead.ids = malloc(sizeof(uint16_t) * 1);
        mhead.ids[0] = 2;

        //computer for this computer.
        struct msg_computer mcomp;
        mcomp.uptime = htonll(1337);
        mcomp.update = htonll(1337);
        mcomp.distance = htons(0);
        mcomp.hostname_count = htons(1);
        char hname1[] = "XXX.XX.X.XXX";
        mcomp.hostnames = malloc(sizeof(char*) * 1);
        mcomp.hostnames[0] = hname1;
        char name[] = "testcomp";
        mcomp.name = name;
        char msg[] = "message - trial";
        mcomp.msg = msg;
        
        size += sizeof(uint16_t) * 2;
        size += sizeof(uint64_t) * 2;
        size += sizeof(uint16_t) * 2;
        size += sizeof(char) * (strlen(mcomp.hostnames[0]) + 1);
        size += sizeof(char) * (strlen(mcomp.name) + 1);
        size += sizeof(char) * (strlen(mcomp.msg) + 1);

        void * data = malloc(size);
        mhead.size = htons(size);
        offset += add_data(data + offset, &mhead.size, sizeof(mhead.size));
        offset += add_data(data + offset, &mhead.count, sizeof(mhead.count));
        offset += add_data(data + offset, &mhead.ids[0], sizeof(uint16_t));
        offset += add_data(data + offset, &mcomp.uptime, sizeof(mcomp.uptime));
        offset += add_data(data + offset, &mcomp.update, sizeof(mcomp.update));
        offset += add_data(data + offset, &mcomp.distance, sizeof(mcomp.distance));
        offset += add_data(data + offset, &mcomp.hostname_count, sizeof(mcomp.hostname_count));
        offset += add_data(data + offset, mcomp.hostnames[0], sizeof(char) * (strlen(mcomp.hostnames[0]) + 1));
        offset += add_data(data + offset, mcomp.name, sizeof(char) * (strlen(mcomp.name) + 1));
        offset += add_data(data + offset, mcomp.msg, sizeof(char) * (strlen(mcomp.msg) + 1));

        //decode from data
        struct msg_header decmhead = decode_packet(data);
        printf("size: %d\ncount: %d\n", decmhead.size, decmhead.count);

        //should write some freeing functions.
        return 0;
}
