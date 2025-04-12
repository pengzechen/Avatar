Timer本身并不复杂，但当我们使用AArch64的timer时，就会发现居然有30多个timer相关的寄存器，到底要用哪个呢，要搞清楚这个，需要先深入了解下AArch64都有哪些timer

AArch64 一共有7个Timer：
cntp： Physical timer
cnthp: Hypervisor physical timer
cntv: Virtual timer
cnthv: Hypervisor virtual timer
cntps: Secure physical timer
cnthps: Secure physical timer
cnthvs: Hypervisor virtual secure timer
除了后3个和安全(EL3)相关，前4个Timer的区别后面会介绍下

这7个timer 的"值"都来源自2个counter

cntpct_el0，是Physical Count register； cntp，cnthp 基于cntpct_el0
cntvct_el0，也就是Virtual Count register；cntv，cnthv 基于cntvct_el0
其实cntvct_el0 和cntpct_el0也是有关系的，取决于hypervisor的配置:

cntvct_el0 = cntpct_el0 - cntvoff_el2
或者cntvct_el0 = cntpct_el0
当然如果硬件没有实现EL2，cntvct_el0 自然也是等于 cntpct_el0
可以把cntpct_el0/cntvct_el0理解为一个递增的时钟滴答，但他不提供任何定时、中断、enable/disable 等功能，提供这些功能的是上面7个Timer

这7个Timer怎么用呢
这7个timer，都有一组3个寄存器，以cnthp为例：

timer value寄存器：cnthp_tval_el2
compare value寄存器：cnthp_cval_el2
控制寄存器：cnthp_ctl_el2
这里解释一下，_el2后缀还是_el0、_el1后缀，取决于这个Timer权限和用途，每个timer只有一个后缀，比如cnthp是_el2，没有cnthp_tval_el0/el1，

_el1后缀的只有 cntps_xxx_el1，当然还有个cntkctl_el1

Timer的中断触发条件是
物理计数器（对于cnthp是：cntpct_el0) - compare value(对于cnthp是:cnthp_cval_el2) >= 0
cnthp_tval_el2怎么用？

简单的讲：

tval存放的是倒计时的值，比如要设置3秒后触发中断，那么就要将 3*cntfrq_el0 写到cnthp_tval_el2这个寄存器里
cval存放的是目标时刻的值，比如当前是10s，设置3s后中断，那么cval要设置为 13*cntfrq_el0
tval和cval是联动的
当写tval时，cval自动被写为 (tval + cntpct_el0)
读tval时，返回的是(cval - cntpct_el0)
所以，tval里的值是递减的，具体的公式：

cnthp_tval_el2 = cnthp_cval_el2 - cntpct_el0
注意：这里 cntpct_el0是针对 cntp和cnthp的，virtual timer的 tval = cval - cntvct_el0

7个timer和物理counter的关系
我们看到，有2个公式了

cntp/hp_tval_elx =  cntp/hp_cval_elx - cntpct_el0
cntv/hv_tval_elx = cntv/hv_cval_elx - cntvct_el0
再加上：

cntvct_el0 = cntpct_el0 - cntvoff_el2