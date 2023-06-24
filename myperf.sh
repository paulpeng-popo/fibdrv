#!/bin/sh

# Add the following to /etc/default/grub
# GRUB_CMDLINE_LINUX_DEFAULT="quiet splash isolcpus=5"
# Reserving CPU 5
# sudo update-grub
# sudo init 6

CPUID=5
MASK=$((1 << $CPUID))
MASK=$((0xfff & ~$MASK))
MASK=`printf "%x" $MASK`
ORIG_ASLR=`cat /proc/sys/kernel/randomize_va_space`
ORIG_GOV=`cat /sys/devices/system/cpu/cpu$CPUID/cpufreq/scaling_governor`
ORIG_TURBO=`cat /sys/devices/system/cpu/intel_pstate/no_turbo`
ORIG_IRQ=`cat /proc/irq/$CPUID/smp_affinity`

# echo "MASK: $MASK"
# echo "ASLR: $ORIG_ASLR"
# echo "Governor: $ORIG_GOV"
# echo "Turbo: $ORIG_TURBO"
# echo "IRQ: $ORIG_IRQ"

# Disable ASLR
sudo sh -c "echo 0 > /proc/sys/kernel/randomize_va_space"

# Disable Turbo Boost
sudo sh -c "echo 1 > /sys/devices/system/cpu/intel_pstate/no_turbo"

# Set CPU governor to performance
sudo sh -c "echo performance > /sys/devices/system/cpu/cpu$CPUID/cpufreq/scaling_governor"

# Disable CPU for handling interrupts
for file in /proc/irq/*/smp_affinity; do
    var=0x$(cat "$file")
    var=$((var & 0xfdf))
    var=`printf "%x" $var`
    sudo sh -c "echo $var > $file" 2> /dev/null
done
sudo sh -c "echo $MASK > /proc/irq/$CPUID/smp_affinity"

# Load the module and run the client
make unload
make load
python3 scripts/statisic_plot.py -cpu=$CPUID
gnuplot -e "filename='scripts/data.txt'" scripts/time_cmp.gp
make unload

# Restore original settings
sudo sh -c "echo $ORIG_ASLR > /proc/sys/kernel/randomize_va_space"
sudo sh -c "echo $ORIG_GOV > /sys/devices/system/cpu/cpu$CPUID/cpufreq/scaling_governor"
sudo sh -c "echo $ORIG_TURBO > /sys/devices/system/cpu/intel_pstate/no_turbo"
sudo sh -c "echo $ORIG_IRQ > /proc/irq/$CPUID/smp_affinity"
