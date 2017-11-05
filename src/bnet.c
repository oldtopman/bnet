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

//If you change these, you need to update the serialization functions as well!!!
struct msg_header{
        uint16_t size;
        uint16_t count;
        uint16_t* ids;
        char ** data;
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
//If you change these, you need to update the serialization functions as well!!!

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

void free_computer(struct msg_computer* mcomp)
{
        for(size_t i = 0; i < mcomp->hostname_count; i++){
                free(mcomp->hostnames[i]);
        }
        free(mcomp->hostnames);
        free(mcomp->name);
        free(mcomp->msg);
}

void free_packet(struct msg_header* mhead)
{
        for(size_t i = 0; i < mhead->count; i++){
                switch(mhead->ids[i]){
                case 2:
                        free_computer((struct msg_computer*)mhead->data[i]);
                        free(mhead->data[i]);
                        break;
                default:
                        //If you're here, you're leaking :c
                        break;
                }
        }
        free(mhead->ids);
        free(mhead->data);
}

void copy_computer(struct msg_computer** dst_ptr, struct msg_computer* src)
{
        *dst_ptr = NULL;
        struct msg_computer* dst = malloc(sizeof *dst);
        *dst = *src;

        dst->name = malloc(strlen(src->name) + 1);
        dst->msg = malloc(strlen(src->msg) + 1);
        dst->hostnames = malloc((sizeof dst->hostnames) * dst->hostname_count);
        if(dst->name == NULL || dst->msg == NULL ||dst->hostnames == NULL){
                free(dst->name);
                free(dst->msg);
                free(dst->hostnames);
                return;
        }

        memcpy(dst->name, src->name, strlen(src->name) + 1);
        memcpy(dst->msg, src->msg, strlen(src->msg) + 1);
        for(uint16_t i = 0; i < src->hostname_count; i++){
                dst->hostnames[i] = malloc(strlen(src->hostnames[i]) + 1);
                if(dst->hostnames[i] == NULL){
                        for(uint16_t j = i + 1; j > 0; j--){
                                free(dst->hostnames[j - 1]);
                        }
                        free(dst->hostnames);
                        return;
                }
                memcpy(dst->hostnames[i], src->hostnames[i], strlen(src->hostnames[i]) + 1);
        }

        *dst_ptr = dst;
}
        

size_t serialize_computer(char** dst, struct msg_computer* src)
{
        size_t hcount = src->hostname_count;
        size_t offset = 0;
        size_t size = 0;
        char* data;

        size += sizeof(uint64_t) * 2;
        size += sizeof(uint16_t) * 2;
        for(size_t i = 0; i < hcount; i++){
                size += strlen(src->hostnames[i]) + 1;
        }
        size += strlen(src->name) + 1;
        size += strlen(src->msg) + 1;
        data = malloc(size);
        if(data == NULL){
                return 0;
        }

        //Convert byte order
        src->uptime = htonll(src->uptime);
        src->update = htonll(src->update);
        src->distance = htons(src->distance);
        src->hostname_count = htons(src->hostname_count);

        offset += add_data(data + offset, &src->uptime, sizeof src->uptime);
        offset += add_data(data + offset, &src->update, sizeof src->update);
        offset += add_data(data + offset, &src->distance, sizeof src->distance);
        offset += add_data(data + offset, &src->hostname_count, sizeof src->hostname_count);
        for(size_t i = 0; i < hcount; i++){
                offset += add_data(data + offset, src->hostnames[i], strlen(src->hostnames[i]) + 1);
        }
        offset += add_data(data + offset, src->name, strlen(src->name) + 1);
        offset += add_data(data + offset, src->msg, strlen(src->msg) + 1);

        *dst = data;
        return size;
}

size_t serialize_packet(char ** dst, struct msg_header src)
{
        uint16_t tmp; //Used when changing byte order
        size_t offset;
        size_t packet_size = 0;
        size_t header_size = 0;
        size_t* data_size;
        char* packet_data;
        char* header_data;
        char** data_data;
        data_size = malloc((sizeof *data_size) * src.count);
        data_data = malloc((sizeof *data_data) * src.count);
        if(data_size == NULL || data_data == NULL){
                free(data_data);
                free(data_size);
                return 0;
        }

        //header
        header_size += sizeof(uint16_t) * 2;
        header_size += sizeof(uint16_t) * src.count;
        header_data = malloc(header_size);
        if(header_data == NULL){
                free(data_data);
                free(data_size);
                return 0;
        }
        offset = 0;
        tmp = htons(src.size);
        offset += add_data(header_data + offset, &tmp, sizeof src.size);
        tmp = htons(src.count);
        offset += add_data(header_data + offset, &tmp , sizeof src.count);
        for(uint16_t i = 0; i < src.count; i++){
                tmp = htons(src.ids[i]);
                offset += add_data(header_data + offset, &tmp, sizeof src.ids[i]);
        }

        //data
        for(uint16_t i = 0; i < src.count; i++){
                switch(src.ids[i]){
                case 2:
                        data_size[i] = serialize_computer(&data_data[i], (struct msg_computer*)src.data[i]);
                        break;
                default:
                        data_size[i] = 0;
                        data_data[i] = NULL;
                        break;
                }
        }

        //Put it all together!
        packet_size = header_size;
        for(size_t i = 0; i < src.count; i++){
                packet_size += data_size[i];
        }

        packet_data = malloc(packet_size);
        if(packet_data == NULL){
                free(header_data);
                free(data_size);
                for(size_t i = 0; i < src.count; i++){
                        free(data_data[i]);
                }
                free(data_data);
                return 0;
        }
        
        offset = 0;
        offset += add_data(packet_data, header_data, header_size);
        for(size_t i = 0; i < src.count; i++){
                offset += add_data(packet_data + offset, data_data[i], data_size[i]);
        }

        //clean up
        free(header_data);
        free(data_size);
        for(size_t i = 0; i < src.count; i++){
                free(data_data[i]);
        }
        free(data_data);

        *dst = packet_data;
        return packet_size;
}

size_t deserialize_computer(char** dst_ptr, char* src)
{
        struct msg_computer* dst = malloc(sizeof *dst);

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

        *dst_ptr = (char*)dst;
        return offset;
}


struct msg_header deserialize_packet(char* data)
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

size_t merge_computers(
                struct msg_computer** dst[],
                struct msg_computer* src1[], size_t count1,
                struct msg_computer* src2[], size_t count2)
{
        //Input sanitization.
        if(dst == NULL || src1 == NULL || src2 == NULL || count1 == 0 || count2 == 0){
                return 0;
        }

        size_t tcount = count1 + count2;
        size_t ucount = tcount;
        struct msg_computer** comps = malloc( (sizeof comps) * tcount);

        //Build list of computers
        if(comps == NULL){
                return 0;
        }
        for(size_t i = 0; i < count1; i++){
                comps[i] = src1[i];
        }
        for(size_t i = 0; i < count2; i++){
                comps[i+count1] = src2[i];
        }

        //Null any duplicates.
        for(size_t i = 0; i < tcount; i++){
                for(size_t j = (i + 1); j < tcount; j++){
                        if(comps[i] != NULL && comps[j] != NULL){
                                if(strcmp(comps[i]->name, comps[j]->name) == 0){
                                        if(comps[i]->update < comps[j]->update){
                                                comps[i] = comps[j];
                                        }
                                        comps[j] = NULL;
                                        ucount--;
                                }
                        }
                }
        }

        //Build combined list.
        size_t counter = 0;
        struct msg_computer** final_comps = malloc( (sizeof final_comps) * ucount);
        if(final_comps == NULL){
                free(comps);
                return 0;
        }
        for(size_t i = 0; i < tcount; i++){
                if(comps[i] != NULL){
                        copy_computer(&final_comps[counter++], comps[i]);
                }
        }
        
        //Both counter and ucount should be identical.
        *dst = final_comps;
        return ucount;
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

        struct msg_header mhead2;
        mhead2.count = 1;
        mhead2.ids = malloc(sizeof(uint16_t) * 1);
        if(mhead2.ids == NULL)
                return 1;
        mhead2.ids[0] = 2;

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

        struct msg_computer mcomp2;
        mcomp2.uptime = 1338;
        mcomp2.update = 1338;
        mcomp2.distance = 0;
        mcomp2.hostname_count = 1;
        char hname12[] = "YYY.YY.Y.YYY";
        mcomp2.hostnames = malloc(sizeof(char*) * 1);
        mcomp2.hostnames[0] = hname12;
        char name2[] = "test2222";
        mcomp2.name = name2;
        char msg2[] = "me22age - 2rial";
        mcomp2.msg = msg2;
        mhead2.data = malloc(sizeof(void*) * 1);
        mhead2.data[0] = (char*)&mcomp2;
        
        size += sizeof(uint16_t) * 3;
        size += sizeof(uint64_t) * 2;
        size += sizeof(uint16_t) * 2;
        size += strlen(mcomp.hostnames[0]) + 1;
        size += strlen(mcomp.name) + 1;
        size += strlen(mcomp.msg) + 1;

        char * data = malloc(size);
        if(data == NULL){
                return 1;
        }
        mhead2.size = size;
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

        char * data2;
        serialize_packet(&data2, mhead2);

        //deserialize from data
        struct msg_header decmhead = deserialize_packet(data);
        printf("size: %d\ncount: %d\nids[0]: %d\n", decmhead.size, decmhead.count, decmhead.ids[0]);
        struct msg_computer* mcp = (struct msg_computer*)decmhead.data[0];
        printf("uptime: %llu\nupdate: %llu\ndistance: %d\nhostname_count: %d\nhostnames[0]: %s\nname: %s\nmsg: %s\n", mcp->uptime, mcp->update, mcp->distance, mcp->hostname_count, mcp->hostnames[0], mcp->name, mcp->msg);

        printf("\n----------------------------\n\n");

        struct msg_header decmhead2 = deserialize_packet(data2);
        printf("size: %d\ncount: %d\nids[0]: %d\n", decmhead2.size, decmhead2.count, decmhead2.ids[0]);
        struct msg_computer* mcp2 = (struct msg_computer*)decmhead2.data[0];
        printf("uptime: %llu\nupdate: %llu\ndistance: %d\nhostname_count: %d\nhostnames[0]: %s\nname: %s\nmsg: %s\n", mcp2->uptime, mcp2->update, mcp2->distance, mcp2->hostname_count, mcp2->hostnames[0], mcp2->name, mcp2->msg);

        struct msg_computer** mergedcomps;
        struct msg_computer** dblmc;
        struct msg_computer* mcpa1[] = {mcp};
        struct msg_computer* mcpa2[] = {mcp2};
        size_t mergedcount = merge_computers(&mergedcomps, mcpa1, 1, mcpa2, 1);
        size_t dblmcount = merge_computers(&dblmc, mergedcomps, mergedcount, mcpa1, 1);
        printf("\n==========================\n\n");
        for(size_t i = 0; i < mergedcount; i++){
                printf("===\n");
                printf("uptime: %llu\nupdate: %llu\ndistance: %d\nhostname_count: %d\nhostnames[0]: %s\nname: %s\nmsg: %s\n", mergedcomps[i]->uptime, mergedcomps[i]->update, mergedcomps[i]->distance, mergedcomps[i]->hostname_count, mergedcomps[i]->hostnames[0], mergedcomps[i]->name, mergedcomps[i]->msg);
        }
        printf("\n==========================\n\n");
        for(size_t i = 0; i < dblmcount; i++){
                printf("===\n");
                printf("uptime: %llu\nupdate: %llu\ndistance: %d\nhostname_count: %d\nhostnames[0]: %s\nname: %s\nmsg: %s\n", dblmc[i]->uptime, dblmc[i]->update, dblmc[i]->distance, dblmc[i]->hostname_count, dblmc[i]->hostnames[0], dblmc[i]->name, dblmc[i]->msg);
        }
        //Free data from the manual generation
        free(mhead.ids);
        free(mhead2.ids);
        free(mcomp.hostnames);
        free(mcomp2.hostnames);
        free(mhead2.data);
        free(data);
        free(data2);

        free_packet(&decmhead2);
        free_packet(&decmhead);
        //manually created packets can't be cleared since they are partly on the stack.
        return 0;
}
