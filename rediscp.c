#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <sys/types.h>
#include <unistd.h>
#include "lib/hiredis.h"
#include "usage.h"

#define SRC 		0
#define DST 		1

/*number of databases.*/
#define DB_MAX		128

#define redisCmd 	redisCommand 
#define freeRpl		freeReplyObject

#define FORCE 		1

typedef struct _redis_{
		redisContext *redis;
		redisReply *reply;
		char *ip;
		int port;
		int cap;
}REDIS;

int dbn = -1;
int force = 0;
REDIS rds[2];


struct timeval timeout = { 3600, 500 }; // 1.5 seconds

static void parseOption(int argc, char* argv[]){

		if(argc <2) goto usage;

		int i;
		for(i = 1 ; i<argc ; i++){

				if(!strcmp(argv[i],"--ipsrc"))
						rds[SRC].ip = argv[++i];
				else if(!strcmp(argv[i],"--portsrc"))
						rds[SRC].port = atoi(argv[++i]);
				else if(!strcmp(argv[i],"--ipdst"))
						rds[DST].ip = argv[++i];
				else if(!strcmp(argv[i],"--portdst"))
						rds[DST].port = atoi(argv[++i]);
				else if(!strcmp(argv[i],"-n")){
						dbn = atoi(argv[++i]);
						if(dbn<0) goto usage;
				}
				else if(!strcmp(argv[i],"-h")){
						goto usage;
				}else if(!strcmp(argv[i],"--force")){
						if(!strcmp(argv[++i],"yes"))
								force = 1;
				}
				else{
						goto usage;
				}
		}
		return ;
usage:
		fprintf(stdout,"%s",USAGE);
		exit(1);
}

static void init_vars(void){

		int i;
		for(i=0;i<2;i++){

				rds[i].redis = redisConnectWithTimeout((char*)rds[i].ip, rds[i].port, timeout);

				if (rds[i].redis->err) {
						printf("Connection error: %s Port:%d\n", rds[i].redis->errstr,rds[i].port);
						exit(1);
				}
				/*
				   redisReply *rp ;
				   rp = redisCmd(rds[i].redis,"CONFIG GET %s","databases");
				   rds[i].cap = atoi(rp->element[1]->str);
				   freeRpl(rp);
				   */

		}
		/*
		   if(force != FORCE && (rds[SRC].cap > rds[DST].cap || dbn > rds[SRC].cap)){
		   fprintf(stderr,"redis src' databases is larger than target's OR specified db index > src's , please fix it.\r\n");
		   goto err;
		   }
		   */
		return ;
		/*
err:
redisFree(rds[SRC].redis);
redisFree(rds[DST].redis);
exit(1);
*/
}

static void import(char * key , redisReply * reply,char* type){

		int i;
		struct redisReply **rpl = reply->element;
		if(!strcmp(type,"string")){
				redisCmd(rds[DST].redis,"SET %s %s",key,reply->str);
		}else if(!strcmp(type,"zset")){

				for(i=0;i<reply->elements;i=i+2)
						redisCmd(rds[DST].redis,"ZADD %s %s %s",key,rpl[i+1]->str,rpl[i]->str);

		}else if(!strcmp(type,"set")){

				for(i=0;i<reply->elements;i++)
						redisCmd(rds[DST].redis,"SADD %s %s",key,rpl[i]->str);

		}else if(!strcmp(type,"hash")){

				for(i=0;i<reply->elements;i=i+2)
						redisCmd(rds[DST].redis,"HSET %s %s %s",key,rpl[i]->str,rpl[i+1]->str);

		}else if(!strcmp(type,"list")){

				for(i=0;i<reply->elements;i++)
						redisCmd(rds[DST].redis,"LPUSH %s %s",key,rpl[i]->str);

		}

		return ;
}

static void export(void){

		int i;
		for(i = 0;i<DB_MAX;i++){

				if(dbn != -1 && i!= dbn) continue;

				/*select SRC db, if failed, reconnect.*/
				redisReply *reply;
				reply = redisCmd(rds[SRC].redis,"SELECT %d",i);

				if(reply->type == REDIS_REPLY_ERROR){
						fprintf(stderr,"Error:%s\r\n",reply->str);
						goto err;
				}
				freeRpl(reply);

				/*select DST db, if failed, reconnect.*/
				reply = redisCmd(rds[DST].redis,"SELECT %d",i);
				if(reply->type == REDIS_REPLY_ERROR){
						fprintf(stderr,"Error:%s\r\n",reply->str);
						goto err;
				}
				freeRpl(reply);
				
				/*get all key.*/
				redisReply *keys;
				keys = redisCmd(rds[SRC].redis,"KEYS *");

				int j;
				for(j=0;j<keys->elements;j++){
						
						char *key = keys->element[j]->str;
						
						/*type.*/
						redisReply *type;
						type = redisCmd(rds[SRC].redis,"type %s",key);

						if(!strcmp(type->str,"string")){

								/*string*/
								redisReply *string = redisCmd(rds[SRC].redis,"GET %s",key);
								import(key,string,"string");
								freeRpl(string);

						}else if(!strcmp(type->str,"zset")){

								/*zset*/
								redisReply *zset = redisCmd(rds[SRC].redis,"ZRANGE %s 0 -1 WITHSCORES",key);
								import(key,zset,"zset");
								freeRpl(zset);

						}else if(!strcmp(type->str,"set")){

								/*set*/
								redisReply *set = redisCmd(rds[SRC].redis,"SMEMBERS %s",key);
								import(key,set,"set");
								freeRpl(set);

						}else if(!strcmp(type->str,"hash")){

								/*hash*/
								redisReply *hash = redisCmd(rds[SRC].redis,"HGETALL %s",key);
								import(key,hash,"hash");
								freeRpl(hash);

						}else if(!strcmp(type->str,"list")){

								/*list*/
								redisReply *lists = redisCmd(rds[SRC].redis,"LRANGE %s 0 -1",key);
								import(key,lists,"list");
								freeRpl(lists);

						}else{
								fprintf(stderr,"UNKNOWN TYPE:%s key:%s\r\n",type->str,key);
								exit(1);
						}
						freeRpl(type);

				}

				freeRpl(keys);

		}
		fprintf(stderr,"Finished.\r\n");
		return ;
err:
		fprintf(stderr,"Or maybe all database was exportd.\r\n");
		redisFree(rds[SRC].redis);
		redisFree(rds[DST].redis);

		exit(1);
}


int main(int argc, char** argv){

		/*parse parameter.*/
		parseOption(argc,argv);

		/*init vars.*/
		init_vars();

		/*export.*/
		export();
		return 0;

}

