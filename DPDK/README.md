1. Load `igb_uio` driver into kernel (using `insmod` or the provided usertool). **NOTE:** refer to this [link](https://askubuntu.com/questions/299676/how-to-install-3rd-party-module-so-that-it-is-loaded-on-boot) to install module permanently.
2. Bind NICs to using the `igb_uio` driver
4. Define, reserve, and mount the hugepages
3. Run DPDK apps...
