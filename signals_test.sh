#!/usr/bin/env bash
# Usage: ./signals_test.sh [part1|part2|part3]
#
# This script tests different parts of the signal handling assignment:
# - part1: Basic signal handling test
# - part2: Tests signals from terminal and between processes
# - part3: Tests blocking signals and pending queues

if [ "$1" == "" ]; then
    echo "Usage: $0 [part1|part2|part3]"
    exit 1
fi

case "$1" in
    part1)
        echo "Running part1 test (output will be saved to outputPart1.txt)"
        {
            ./problem1_part1 &
            PARENT=$!
            echo "Started problem1_part1 (PID=$PARENT)"
            echo "Sending a few test signals..."
            sleep 3
            
            # Send a few example signals
            kill -INT $PARENT
            sleep 1
            kill -ABRT $PARENT
            sleep 1
            
            wait $PARENT
        } 2>&1 | tee outputPart1.txt
        echo "Test completed. Results saved to outputPart1.txt"
        ;;
        
    part2)
        echo "Running part2 test (output will be saved to outputPart2.txt)"
        {
            ./problem1_part2 &
            PARENT=$!
            echo "Started problem1_part2 (PID=$PARENT)"
            echo "Sending signals from terminal to parent..."
            sleep 3
            
            # Send each signal once from terminal to parent
            for sig in INT ABRT ILL CHLD SEGV FPE HUP TSTP; do
                echo "Sending signal $sig to parent"
                kill -$sig $PARENT
                sleep 2
            done
            
            echo "Now sending each signal multiple times..."
            for sig in INT ABRT ILL CHLD SEGV FPE HUP TSTP; do
                echo "Sending signal $sig to parent (3 times)"
                for i in 1 2 3; do
                    kill -$sig $PARENT
                    sleep 0.2
                done
                sleep 1
            done
            
            wait $PARENT
        } 2>&1 | tee outputPart2.txt
        echo "Test completed. Results saved to outputPart2.txt"
        ;;
        
    part3)
        echo "Running part3 test (output will be saved to outputPart3.txt)"
        {
            ./problem1_part3 &
            PARENT=$!
            echo "Started problem1_part3 (PID=$PARENT)"
            sleep 1

            for sig in INT QUIT TSTP ABRT ILL CHLD SEGV FPE HUP; do
                for i in 1 2 3; do
                    kill -$sig $PARENT
                done
            done

            sleep 2

            CHILDREN=$(pgrep -P $PARENT)
            for c in $CHILDREN; do
                echo "Sending to child $c"
                for sig in INT QUIT TSTP ABRT ILL CHLD SEGV FPE HUP; do
                    for i in 1 2 3; do
                        kill -$sig $c
                    done
                done
            done

            wait $PARENT
        } 2>&1 | tee outputPart3.txt
        echo "Test completed. Results saved to outputPart3.txt"
        ;;
        
    *)
        echo "Unknown part: $1"
        echo "Usage: $0 [part1|part2|part3]"
        exit 1
        ;;
esac
