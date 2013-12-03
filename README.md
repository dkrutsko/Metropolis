# Metropolis

<p align="justify">This is a simple implementation of the Neighbor Discovery Protocol (NDP). This application supports up to a maximum of 32 neighbors after which new neighbors get discarded until the previous ones disappear. To support more than 32 neighbors, simply modify the #define in NDP.h and recompile. Be careful though, as higher values could break the interface. Under normal situations, beacon packets are sent every three seconds.</p>

### Running
```bash
$ make
$ sudo ./Metropolis
```

### Requires
* ncurses
* pthreads

### Usage
* Use arrow keys to navigate the menu
* Press enter to make a selection
* Enter an interface to use (iwconfig)
* Press Q during the protocol to stop and return to the menu
* Press F during the protocol to toggle Stress Testing mode

### Stress Testing

<p align="justify">Stress Testing mode sends a flood of beacon packets with randomized source addresses allowing you to stress test systems with large numbers of neighbors. May not work on restricted systems.</p>

### Authors
**D. Krutsko**

* Email: <dave@krutsko.net>
* Home: [dave.krutsko.net](http://dave.krutsko.net)
* GitHub: [github.com/dkrutsko](https://github.com/dkrutsko)

**S. Schneider**

* Email: <seb.schneider84@gmail.com>
* GitHub: [github.com/Harrold](https://github.com/Harrold)

**A. Shukla**

* Email: <abstronomer@gmail.com>
* GitHub: [github.com/AbsMechanik](https://github.com/AbsMechanik)