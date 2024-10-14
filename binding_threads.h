/**
 * This code provides an interface for setting thread affinity (binding a thread to a specific CPU core) on a macOS system,
 * Since macOS doesn't directly support CPU affinity using pthread_setaffinity_np, it achieves similar functionality by
 * interacting with macOS;s Mach kernel interfaces.
 * This code provides functionality for working with CPU affinity on macOS, where POSIX functions like pthread_setaffinity_np 
 * aren't natively available.
 * The sched_getaffinity function sets up the CPU set by querying the number of cores using the sysctl interface.
 * The pthread_setaffinity_np function binds a thread to a specific core by interacting with the Mach kernel, 
 * using Mach-specific functions like thread_policy_set to control thread affinity.
 */
#pragma once
#include <iostream>
// Provides access to system control values, which allow querying an setting system parameters (e.g. CPU core count)
#include <sys/sysctl.h>
// These headers are part of the Mach kernel, used to manage threads and CPU cores at a low level on macOS
#include <mach/mach_types.h>
#include <mach/mach_init.h>
#include <mach/mach.h>
#include <mach/thread_policy.h>

/*
 * This macro defines the system control key for retrieving the number of CPU cores on the macOS
 * It uses the sysctlbyname function to access the machdep.cpu.core_count parameter which stores the number of cores on the system
 */ 
#define SYSCTL_CORE_COUNT "machdep.cpu.core_count"

// The Common namespace encapsulates the utility functions and types to avoid polluting the global namespace
namespace Common{

    /**
     * The cpu_set_t structure is defined to keep track of the CPU cores where a thread is allowed to run
     *  Each it of the 32-bit unsigned integer 'count' represents a CPU core
     */
    typedef struct cpu_set{
        uint32_t count;
    } cpu_set_t;

    /**
     * This function clears all the bits in the cpu_set_t structure, meaning no cores are selected for affinity
     */
    static inline void CPU_ZERO(cpu_set_t *cs){
        cs->count = 0;
    }

    /**
     * This function sets the bit corresponding to the core num in the cpu_set_t structure
     * This indicates that the thread can run on core num
     */
    static inline void CPU_SET(int num, cpu_set_t *cs){
        cs->count |= (1<<num);
    }

    /**
     * This function checks if core num is set in the cpu_set_t structure
     * It returns a non-zero value if the thread is allowed to run on core num and zero otherwise
     */
    static inline int CPU_ISSET(int num, cpu_set_t *cs){
        return (cs->count & (1 << num));
    }

    /**
     * This function retrieves the number of CPU cores on the system and sets all of them in the cpu_set_t structure
     * Consequently, the thread can run on any core
     */
    int sched_getaffinity(pid_t pid, size_t cpu_size, cpu_set_t *cpu_set){
        int32_t core_count = 0;
        size_t len = sizeof(core_count);

        // The number of CPU cores is retrieved using the machdep.cpu.core_count sysctl parameter
        int ret = sysctlbyname(SYSCTL_CORE_COUNT, &core_count, &len, 0, 0);
        if(ret){
            printf("error while getting count %d\n", ret);
            return -1;
        }

        cpu_set->count = 0;

        // The for loop sets each bit corresponding to a core in the cpu_set_t up to core_count
        for(int i = 0; i< core_count; i++){
            cpu_set->count |= (1<<i);
        }

        // Return o on success and -1 on failure
        return 0;
    }

    /**
     * This functions sets the CPU affinity for a given thread, binding it to the first available core in the cpu_set_t structure.
     */
    int pthread_setaffinity_np(pthread_t thread, size_t cpu_size, cpu_set_t *cpu_set){
        thread_port_t mach_thread;
        int core  = 0;

        // Looping through the cores in cpu_set_t using CPU_ISSET to find the first core that is set. 
        // The thread will be bound to this core
        for(core = 0; core< (8 * cpu_size); core++){
            if(CPU_ISSET(core, cpu_set)) break;
        }

        printf("binding to core %d\n", core);

        // Defines the affinity policy, specifying the core to which the thread will be bound.
        thread_affinity_policy_data_t policy = {core};

        // Converting a POSIX thread (pthread_t) to a Mach thread (thread_port_t) which is requires to interact with the Mach kernel
        mach_thread = pthread_mach_thread_np(thread);

        // Sets the thread's affinity policy, pinning it to the specified core.
        thread_policy_set(mach_thread, THREAD_AFFINITY_POLICY, (thread_policy_t) &policy, 1);
        
        return 0;

    }
}
