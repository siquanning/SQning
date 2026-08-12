package require ::quartus::jtag

set hardware [lindex [get_hardware_names] 0]
set device [lindex [get_device_names -hardware_name $hardware] 0]
puts "Hardware: $hardware"
puts "Device: $device"

# EP4CE10F17: IR=10 bits, SAMPLE opcode=0000000101, boundary register=603 bits.
# Boundary input cells from the Quartus-generated BSDL file.
array set pin_bit {
    A3_IN 12
    A5_IN 36
    A6_IN 48
    B3_IN 9
    B5_PAD 27
    B5_CORE 29
    B6_PAD 45
    B6_CORE 47
    B7_PAD 60
    B7_CORE 62
    A4_PAD 24
    A4_CORE 26
    P3_PAD 474
    P3_CORE 476
    R3_PAD 471
    R3_CORE 473
    R4_PAD 453
    R4_CORE 455
    R5_PAD 429
    R5_CORE 431
}
set pins {A3_IN A5_IN A6_IN B3_IN B5_PAD B5_CORE B6_PAD B6_CORE B7_PAD B7_CORE A4_PAD A4_CORE P3_PAD P3_CORE R3_PAD R3_CORE R4_PAD R4_CORE R5_PAD R5_CORE}
foreach pin $pins {
    set ones($pin) 0
    set transitions($pin) 0
    set previous($pin) X
}
foreach state {00 01 10 11} {
    set input_pair($state) 0
    set output_pair($state) 0
}

open_device -hardware_name $hardware -device_name $device
if {[catch {
    device_lock -timeout 10000
    device_ir_shift -ir_value 0x5 -no_captured_ir_value
    set zeros [string repeat 0 603]
    for {set sample 0} {$sample < 1000} {incr sample} {
        set captured [device_dr_shift -length 603 -dr_value $zeros]
        foreach pin $pins {
            # Cell zero is the first TDO bit and is returned at the right edge.
            set value [string index $captured [expr 602 - $pin_bit($pin)]]
            if {$value eq "1"} {
                incr ones($pin)
            }
            if {$previous($pin) ne "X" && $value ne $previous($pin)} {
                incr transitions($pin)
            }
            set current($pin) $value
            set previous($pin) $value
        }
        set in_state "$current(A3_IN)$current(A5_IN)"
        set out_state "$current(B5_PAD)$current(B6_PAD)"
        incr input_pair($in_state)
        incr output_pair($out_state)
    }
    device_unlock
} error_message]} {
    catch {device_unlock}
    close_device
    error $error_message
}
close_device

puts "Samples: 1000"
foreach pin $pins {
    puts [format "%s ones=%d transitions=%d" $pin $ones($pin) $transitions($pin)]
}
foreach state {00 01 10 11} {
    puts "A3/A5 $state=$input_pair($state)"
}
foreach state {00 01 10 11} {
    puts "B5/B6 $state=$output_pair($state)"
}
