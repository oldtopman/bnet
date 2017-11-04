#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

#ifdef __linux__
#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif
#include <endian.h>
#define ntohll(x) be64toh(x)
#define htonll(x) htobe64(x)
#endif

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

size_t add_data(void * dst, void * src, size_t size)
{
        if (dst == NULL || src == NULL || size == 0){
                return 0;
        }
        memcpy(dst, src, size);
        return size;
}

size_t get_string(char ** dst, void * src)
{
        size_t size = strlen(src) + 1;
        *dst = malloc(size);
        if(*dst == NULL){
                return 0;
        }

        memcpy(*dst, src, size);
        return size;
}

size_t serialize_computer(void ** dst, struct msg_computer src)
{
        return 0;
}

size_t serialize_packet(void ** dst, struct msg_header src)
{
        return 0;
}

size_t deserialize_computer(struct msg_computer** dst_ptr, void * src)
{
        struct msg_computer* dst = malloc(sizeof *dst);
        *dst_ptr = dst;

        if(src == NULL || dst == NULL){
                return 0;
        }

        size_t offset = 0;

        offset += add_data(&dst->uptime, src + offset, sizeof dst->uptime);
        dst->uptime = ntohll(dst->uptime);
        offset += add_data(&dst->update, src + offset, sizeof dst->update);
        dst->update = ntohll(dst->update);
        offset += add_data(&dst->distance, src + offset, sizeof dst->distance);
        dst->distance = ntohs(dst->distance);
        offset += add_data(&dst->hostname_count, src + offset, sizeof dst->hostname_count);
        dst->hostname_count = ntohs(dst->hostname_count);

        //Get number of hostnames (required for calculating size)
        if(dst->hostname_count > 0){
                dst->hostnames = malloc((sizeof *dst->hostnames) * dst->hostname_count);
                if(dst->hostnames == NULL){
                        free(dst);
                        return 0;
                }

                for(int i = 0; i < dst->hostname_count; i++){
                        offset += get_string(&dst->hostnames[i], src + offset);
                }
        }

        offset += get_string(&dst->name, src + offset);
        offset += get_string(&dst->msg, src + offset);

        return offset;
}


struct msg_header deserialize_packet(void* data)
{
        struct msg_header mhead;
        if(data == NULL){
                mhead.size = 0;
                mhead.count = 0;
                mhead.ids = NULL;
                mhead.data = NULL;
                return mhead;
        }

        //Get header info.
        size_t offset = 0;
        offset += add_data(&mhead.size, data + offset, sizeof mhead.size);
        mhead.size = ntohs(mhead.size);
        offset += add_data(&mhead.count, data + offset, sizeof mhead.count);
        mhead.count = ntohs(mhead.count);

        //If there are no objects, we're done. And the packet was useless.
        if(mhead.count == 0){
                mhead.ids = NULL;
                mhead.data = NULL;
                return mhead;
        }

        //Get ids
        mhead.ids = malloc((sizeof *mhead.ids) * mhead.count);
        if(mhead.ids == NULL){
                mhead.size = 0;
                mhead.count = 0;
                mhead.ids = NULL;
                mhead.data = NULL;
                return mhead;
        }
        for(size_t i = 0; i < mhead.count; i++){
                offset += add_data(mhead.ids + i, data + offset, sizeof *mhead.ids);
                mhead.ids[i] = ntohs(mhead.ids[i]);
        }

        //Get data objects and read into data array
        mhead.data = malloc((sizeof *mhead.data) * mhead.count);
        if(mhead.data == NULL){
                free(mhead.ids);
                mhead.size = 0;
                mhead.count = 0;
                mhead.ids = NULL;
                mhead.data = NULL;
                return mhead;
        }

        for(size_t i = 0; i < mhead.count; i++){
                switch(mhead.ids[i]){
                case 2:
                        offset += deserialize_computer(&mhead.data[i], data + offset);
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
        size_t offset = 0;
        uint16_t size = 0;

        //Build one packet for this computer.
        struct msg_header mhead;
        mhead.count = htons(1);
        mhead.ids = malloc(sizeof(uint16_t) * 1);
        if(mhead.ids == NULL)
                return 1;
        mhead.ids[0] = htons(2);

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
        size += strlen(mcomp.hostnames[0]) + 1;
        size += strlen(mcomp.name) + 1;
        size += strlen(mcomp.msg) + 1;

        void * data = malloc(size);
        if(data == NULL){
                return 1;
        }
        mhead.size = htons(size);
        offset += add_data(data + offset, &mhead.size, sizeof(mhead.size));
        offset += add_data(data + offset, &mhead.count, sizeof(mhead.count));
        offset += add_data(data + offset, &mhead.ids[0], sizeof(uint16_t));
        offset += add_data(data + offset, &mcomp.uptime, sizeof(mcomp.uptime));
        offset += add_data(data + offset, &mcomp.update, sizeof(mcomp.update));
        offset += add_data(data + offset, &mcomp.distance, sizeof(mcomp.distance));
        offset += add_data(data + offset, &mcomp.hostname_count, sizeof(mcomp.hostname_count));
        offset += add_data(data + offset, mcomp.hostnames[0], strlen(mcomp.hostnames[0]) + 1);
        offset += add_data(data + offset, mcomp.name, strlen(mcomp.name) + 1);
        offset += add_data(data + offset, mcomp.msg, strlen(mcomp.msg) + 1);

        void * data2;
        size_t size2 = serialize_packet(data2, mhead);

        //deserialize from data
        struct msg_header decmhead = deserialize_packet(data);
        printf("size: %d\ncount: %d\nids[0]: %d\n", decmhead.size, decmhead.count, decmhead.ids[0]);
        struct msg_computer* mcp = decmhead.data[0];
        printf("uptime: %d\nupdate: %d\ndistance: %d\nhostname_count: %d\nhostnames[0]: %s\nname: %s\nmsg: %s\n", mcp->uptime, mcp->update, mcp->distance, mcp->hostname_count, mcp->hostnames[0], mcp->name, mcp->msg);

        //should write some freeing functions.
        return 0;
}
