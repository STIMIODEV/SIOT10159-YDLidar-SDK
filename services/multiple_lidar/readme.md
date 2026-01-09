# Installing the binary

You need to compile and install the application "multiple_lidar" on your /usb/bin folder to use the services : 

```
cd /path/to/your/git/repos/SIOT10159-YDLidar-SDK/build
make
sudo cp /path/to/your/git/repos/SIOT10159-YDLidar-SDK/build/multiple_lidar /usb/bin/
```

# preparing service :

```
sudo cp *.service /etc/systemd/system
sudo cp *.target /etc/systemd/system
systemctl restart daemon-reload
```

# starting service : 
```
systemctl start multiple_lidar_barre_1.target
```

SUDO password may be asked.

You can check if the 6 programs are running (it may takes some times) by using this command : 

```
ps -aux -ww | grep multi
```

```
systemctl stop multiple_lidar_barre_1.target
```

