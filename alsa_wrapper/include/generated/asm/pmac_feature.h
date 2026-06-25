#define PMAC_FTR_DEF(x) x

#define PMAC_FTR_READ_GPIO   PMAC_FTR_DEF(17)
#define PMAC_FTR_WRITE_GPIO  PMAC_FTR_DEF(18)

static inline long pmac_call_feature(int feature, void *node, long param, long value) {
    #ifdef CONFIG_X86
    #endif
    return 0; 
}
