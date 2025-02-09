# Simon Sepiol-Duchemin, Joshua Setia
#!/bin/bash

# Output and error files to log results
output_file="output_n30.txt"
error_file="error_log30.txt"

# Clear the output and error files
> $output_file
> $error_file

echo "Contenu de \$OAR_NODEFILE:" >> $output_file
cat $OAR_NODEFILE >> $output_file

# Function to run mpiexec with a specified number of processes
echo "Starting test runs..." >> $output_file

function run_mpi() {
    local n=$1
    local run_index=1

    echo "Running mpiexec with --n $n" >> $output_file

    while [ $run_index -le 5 ]; do
        #echo "Attempting run #$run_index for $n processes:" >> $output_file
        mpiexec --n $n --mca pml ^ucx --hostfile $OAR_NODEFILE ./mpi_mitm --n 30 --C0 797b45fb4d863eca --C1 d50beaf32a0f27eb >> $output_file 2>> $error_file


        if [ $? -eq 0 ]; then
            echo "Run #$run_index for $n processes completed successfully." >> $output_file
            run_index=$((run_index + 1))
        else
            echo "Error occurred during run #$run_index for $n processes. Retrying..." >> $output_file
            echo "Detailed error:" >> $error_file
            sleep 2  # Pause before retrying
        fi
    done

    echo "Completed 5 successful runs for --n $n" >> $output_file
}

# Run for processes from 2 to 10
for n in $(seq 2 1 10); do
    run_mpi $n
    echo "Completed runs for $n processes." >> $output_file
done

# Run for processes from 20 to 150 in steps of 10
for n in $(seq 20 10 150); do
    run_mpi $n
    echo "Completed runs for $n processes." >> $output_file
done

echo "All test runs completed." >> $output_file
