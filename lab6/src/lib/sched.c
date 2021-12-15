#include <env.h>
#include <pmap.h>
#include <printf.h>

/* Overview:
 *  Implement simple round-robin scheduling.
 *  Search through 'envs' for a runnable environment ,
 *  in circular fashion statrting after the previously running env,
 *  and switch to the first such environment found.
 *
 * Hints:
 *  The variable which is for counting should be defined as 'static'.
 */
// void sched_yield(void)
// {
//   static u_int cur_lasttime = 1;
//   static int cur_head_index = 0;
//   struct Env *next_env;
//   --cur_lasttime;
//   if(cur_lasttime == 0 || curenv == NULL) {
//     if(curenv) {
//       LIST_INSERT_HEAD(&env_sched_list[!cur_head_index], curenv, env_sched_link);
//     }
//     if(LIST_EMPTY(&env_sched_list[cur_head_index])) {
//       cur_head_index = !cur_head_index;
//     }
//     if(LIST_EMPTY(&env_sched_list[cur_head_index])) {
//       panic("No env is RUNNABLE!");
//     }
//     next_env = LIST_FIRST(&env_sched_list[cur_head_index]);
//     LIST_REMOVE(next_env, env_sched_link);
//     cur_lasttime = next_env->env_pri;
//       env_run(next_env);
//   }
//   env_run(curenv);
//   panic("sched yield reached end. ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^");
// }
void sched_yield(void)
{
    //struct Env *e;
    static struct Env *cur = NULL;
    static int num = 0; // 当前正在遍历的链表
    static int count = 0; // 当前进程剩余执行次数
    int IsChange = 0; // Lab6需要用到，其他不同管
    while( (!cur) || count <= 0 || (cur && cur->env_status != ENV_RUNNABLE)) {
        count = 0;
        if(cur != NULL) {
            LIST_REMOVE(cur, env_sched_link);
            if(cur->env_status != ENV_FREE)
                LIST_INSERT_HEAD(&env_sched_list[num^1], cur, env_sched_link);
        }
        if(LIST_EMPTY(&env_sched_list[num])) num ^= 1;
        if(LIST_EMPTY(&env_sched_list[num])) {
            //printf("\nsched.c/sched_yield:NO envs to run\n");
            continue ;
        }
        cur = LIST_FIRST(&env_sched_list[num]);
        if(cur != NULL) count = cur->env_pri;
        IsChange = 1;
        //printf("%x %d\n", cur, cur->env_status);
    }
    if(IsChange) ++cur->env_runs; //Lab6
    -count;
    env_run(cur);
}
