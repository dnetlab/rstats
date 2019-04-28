
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

#include "rstats.h"
#include "adapter.h"

static long g_uptime_now = 0;
static long g_uptime_old = 0;

static volatile sig_atomic_t gothup = 0;
static volatile sig_atomic_t gotusr1 = 0;
static volatile sig_atomic_t gotusr2 = 0;

static stats_item_list_t g_stats_list;

long uptime()
{
    struct sysinfo info;
    sysinfo(&info);
    return info.uptime;
}

char *lan_vif_to_name(char *ifname)
{
    int idx = 0;
    static char name[20];

    sscanf(ifname, "br%d", &idx);
    snprintf(name, sizeof(name), "LAN%d", idx + 1);

    return name;
}

int switch_phyport_bytes(int port, uint64_t *rx, uint64_t *tx)
{
    int ret = 0;
    FILE *fp = NULL;
    char cmd[128] = {0};
    char line[128] = {0};
    uint64_t count[MAX_COUNTER] = {0};

    snprintf(cmd, sizeof(cmd), "swconfig dev switch0 port %d get mib", port);
    fp = popen(cmd, "r");
    if(!fp)
    {
        perror("popen\n");
        return -1;
    }

    while(fgets(line, sizeof(line), fp))
    {    
        if(sscanf(line, "RxGoodByte  : %llu", &count[0]) == 1)
        {
            *rx = count[0];
        }

        if(sscanf(line, "TxByte      : %llu", &count[1]) == 1)
        {
            *tx = count[1];
        }
    }

    pclose(fp);

    return 0;
}

static int update_netdev(const char *ifdesc, uint64_t *counter)
{
    int ret = 0;
    stats_item_t *item, *tmp = NULL;

    list_for_each_entry(item, &g_stats_list.head, list) 
    {
        if(strcmp(ifdesc, item->ifdesc) == 0)
        {
            tmp = item; 
        }
    }

    if(!tmp)
    {
        tmp = malloc(sizeof(stats_item_t));
        if(tmp)
        {
            memset(tmp, 0x0, sizeof(stats_item_t));
            
            strncpy(tmp->ifdesc, ifdesc, sizeof(tmp->ifdesc) - 1);

            tmp->head = 0;
            tmp->tail = 1;
            tmp->count = 1;
            tmp->stats[1][0] = counter[0];
            tmp->stats[1][1] = counter[1];

            INIT_LIST_HEAD(&tmp->list);
            list_add(&tmp->list, &g_stats_list.head);
            g_stats_list.count ++;
        }
        
        return 0;
    }

    if(tmp->count >= MAX_NSTATS)
    {
        tmp->count = MAX_NSTATS;
    }
    else
    {
        tmp->count ++;
    }

    if(tmp->count == MAX_NSTATS)
    {
        if((tmp->head + 1) == MAX_NSTATS)
        {
            tmp->head = 0;
        }
        else
        {
            tmp->head ++;
        }
    }
    
    if((tmp->tail + 1) == MAX_NSTATS)
    {
        tmp->tail = 0;
    }
    else
    {
        tmp->tail ++;
    }

    tmp->stats[tmp->tail][0] = counter[0];
    tmp->stats[tmp->tail][1] = counter[1];

    return 0;
}

static void process_rstats()
{
    int i = 0;
    FILE *fp = NULL;
    char buff[256] = {0};
    char ifname[20] = {0};
    uint64_t counter[MAX_COUNTER];
	long tick, ticks;

    tick = g_uptime_now - g_uptime_old;
	ticks = tick / RSTATS_INTERVAL;
    
	fp = fopen("/proc/net/dev", "r");
	if (fp)
    {
		fgets(buff, sizeof(buff), fp);
		fgets(buff, sizeof(buff), fp);
        
		while (fgets(buff, sizeof(buff), fp)) 
        {
			if (sscanf(buff, " %[^:]:%llu%*u%*u%*u%*u%*u%*u%*u%llu", ifname, &counter[0], &counter[1]) != 3)
				continue;

            /* 默认统计LAN和WAN的数据 */
            if(!strstr(ifname, "br"))
                continue;

            if(strcmp(ifname, "brwan") == 0)
            {   
			    update_netdev("WAN", counter);
            }
            else
            {
                update_netdev(lan_vif_to_name(ifname), counter);
            }
		}
        
		fclose(fp);
	}

    /* 端口统计 */
    for(i = 1; i <= MAX_PANNEL_PORT; i ++)
    {
        switch_phyport_bytes(i, &counter[0], &counter[1]);
        snprintf(buff, sizeof(buff), "port%d", phyPort_to_pannelPort_xlate(i));
        update_netdev(buff, counter);
    }

	g_uptime_old += (ticks * RSTATS_INTERVAL);    
}

void alloc_rstats()
{
    INIT_LIST_HEAD(&g_stats_list.head);
    g_stats_list.count = 0;
    g_stats_list.tail = 0;
}

static void catch_sig_rstats(int sig)
{
	switch(sig)
    {
    	case SIGUSR1:
    		gotusr1 = 1;
    		break;
	}
}

void dump_stats_json()
{
    int i = 0;
    int j = 0;
    FILE *fp = NULL;
    stats_item_t *item;
    int head = 0, tail = 0;
    int val1 = 0, val2 = 0;

    int start, end;

    fp = fopen(RSTATS_DATA_QUERY, "r");
    if(fp)
    {
        fscanf(fp, "start:%d end:%d", &start, &end);
        fclose(fp);

        start = start * 12;
        end = end * 12;
    }
    else
    {
        start = 0;
        end = MAX_NSTATS; 
    }

    //printf("start = %d\n", start);
    //printf("end = %d\n", end);

    fp = fopen(RSTATS_DATA_FILE, "w");
    if(!fp)
    {
        return;
    }

    fprintf(fp, "{\"rstats\":[");
    
    list_for_each_entry(item, &g_stats_list.head, list)
    {
        printf("\n");
    
        tail = item->tail;
        head = item->head;

        //printf("tail = %d\n", tail);
        //printf("head = %d\n", head);
        
        if(tail - start < 0)
        {
            val1 = tail + MAX_NSTATS - start;
        }
        else
        {
            val1 = tail - start;            
        }

        if(tail - end < 0)
        {
            val2 = tail + MAX_NSTATS - end;
        }
        else
        {
            val2 = tail - end;
        }

        //printf("val1 = %d\n", val1);
        //printf("val2 = %d\n", val2);

        fprintf(fp, "%s{\"ifdesc\":\"%s\",\"meter\":[", ((j > 0) ? ",": ""), item->ifdesc);
        
        for(i = 0; i < MAX_NSTATS; i ++)
        {
            if(val1 == val2)
            {
                break;
            }

            fprintf(fp, "%s{\"rx\":%llu,\"tx\":%llu}", ((i > 0) ? "," : ""), item->stats[val1][0], item->stats[val1][1]);

            val1 --;
            
            if(val1 < 0)
            {
                val1 = MAX_NSTATS - 1;
            }
        }

        fprintf(fp, "]}");
        
        j ++;
    }
    
    fprintf(fp, "]}");

    fclose(fp);
}

int main(int argc, char *argv[])
{
    FILE *fp = NULL;    
    pid_t pid = 0;
	long z;

    signal(SIGUSR1, catch_sig_rstats);

    if(daemon(0, 0) < 0)
    {
        perror("daemon");
        exit(1);
    }

    alloc_rstats();

    /* 写入PID，用于其他进程获取 */
    pid = getpid();
    if((fp = fopen(RSTATS_PID_FILE, "w")) != NULL)  
    {
        fprintf(fp, "%d", pid);
        fclose(fp);
    }

	z = uptime();
    
	g_uptime_now = z;
	g_uptime_old = z;

    while(1)
    {    
        while (g_uptime_now < z)
        {    
            sleep(z - g_uptime_now);

            if (gotusr1)
            {
                dump_stats_json();
                gotusr1 = 0;
            }

            g_uptime_now = uptime();
        }

        process_rstats();
        
		z += RSTATS_INTERVAL;
    }

    return 0;
}
