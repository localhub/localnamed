# localnamed

Lets any process request a local hostname. Just open a connection to `/var/run/localnamed.sock`. Writing a line like this:

    +myprocess.local

…adds `myprocess.local` to `/etc/hosts` pointed at `127.0.0.1`. Writing a line like this:

    -myprocess.local

…unregisters it. When you close your connection to localnamed (e.g. when your process exits), all of your hostnames are deregistered.

This is great for testing web servers that depend on the value of the `Host` header.

## Platforms

Currently just OS X, because it uses launchd to set up its socket. systemd and (I believe) upstart have similar facilities, though, so porting it to Linux should be dang easy.

## Install

- Clone this repository
- `make && sudo make install`

## Uninstall

- `sudo make uninstall`
