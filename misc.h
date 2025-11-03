#pragma once

/**
 * @brief Handles user input 
 * @param argc - The number of args
 * @param argv - an array of strings as (char**)
 * @param hostname - the name of the host we are looking up
 * @param dns_ip - the ip address of the DNS we are going to use for the lookup
 * @return True on sucess, otherwise false
 */
bool handleUserInput(int argc, char** argv, char*& hostname, char*& dns_ip);

/**
 * @brief Start Win Socket API
 * @return True on success, otherwise false
 */
bool startWSA();


clock_t clockDelta(clock_t startTime, clock_t endTime);