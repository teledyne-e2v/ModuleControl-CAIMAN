# ModuleControl
Simple code to start controlling Emerald-Gen2 module and play with some Gstreamer plugins


# Release Notes
- first version for Emerald-gen2 (tested on orin nano)
- implement focus dac control for liquid lens

# Compilation
IMPORTANT: make sure the time clock and date are correct in the Jetson before any compilation

	sudo apt update
	sudo apt install libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev

Recompile the sources:

	make

# Gstreamer plugins
The following Gstreamer plugins are comaptible with this application (not mandatory)
- gst-autofocus: https://github.com/teledyne-e2v/gst-autoexposure
- gst-bacode-reader: https://github.com/teledyne-e2v/gst-barcode-reader

Please following the associated instructions to install them.
In order to have acces to the plugins when terminal is opened, add the following lines at the end of the file: ```~/.bashrc```

	export GST_PLUGIN_PATH=/usr/local/lib/gstreamer-1.0/
	export LD_LIBRARY_PATH=/usr/local/lib/

After resrting the terminal, test the plugin installation with:

	gst-inspect-1.0 autofocus
	gst-inspect-1.0 barcodereader

# Application manual
Make sure to be part of i2c group with `groups` command, if not add it with:

	sudo usermod -aG i2c $USER

Open the two applications in separated terminals:

### terminal 1: control module I2C bus

	./ModuleControl

### terminal 2: control GSTREAMER vieo stream I2C bus

	./gstCommands

**IMPORTANT RULES:**
- it is required to have the two applications started at the same time
- the video stream must be started BEFORE controlling image sensor through I2C bus


## Additionnal tips:
Shortcut can also be added in the desktop:
- start_gstCommands.desktop
- start_ModuleControl.desktop

VNC remote control can be put in place with: https://developer.nvidia.com/embedded/learn/tutorials/vnc-setup

	cd /usr/lib/systemd/user/graphical-session.target.wants
	sudo ln -s ../vino-server.service ./.
	gsettings set org.gnome.Vino prompt-enabled false
	gsettings set org.gnome.Vino require-encryption false
	gsettings set org.gnome.Vino authentication-methods "['vnc']"
	gsettings set org.gnome.Vino vnc-password $(echo -n 'teledyne2022'|base64)
	sudo reboot

Connect using jetson IP address (not port definition needed), accessible with

	ip a


