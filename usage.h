#ifndef __USAGE__
#define __USAGE__
#define USAGE		"\nUsage: ./rediscp [OPTIONS] [cmd [arg [arg ...]]]\n\n\
	--ipsrc         source ip \n\
	--portsrc       source port \n\
	--ipdst         target ip\n\
	--portdst       target port\n\
	-n              databases index, all databases as default.\n\n\
EXAMPLE:\n\
	./rediscp --ipsrc ec2.dousl.com --portsrc 6379 --ipdst ec2.dousl.com --portdst 6380 -n 0\n\n"
#endif
