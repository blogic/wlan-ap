# Doing a native build on Linux
First we need to clone and setup our tree. This will result in an openwrt/.
```
python3 setup.py --setup
```
Next we need to select the profile and base package selection. This setup will install the feeds, packages and generate the .config file. The available profiles are ap2220, ea8300, ecw5211, ecw5410.
```
cd openwrt
./scripts/gen_config.py ap2220 wlan-ap
```
Finally we can build the tree.
```
make -j X V=s
```
Builds for different profiles can co-exist in the same tree. Switching is done by simple calling gen_config.py again.


