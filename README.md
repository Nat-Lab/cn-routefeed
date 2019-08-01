cn-routefeed: China Routefeed
---

`cn-routefeed` is a BGP speaker that feeds all China IPv4 delegations to peer. The delegation information is fetched from APNIC (from <http://ftp.apnic.net/apnic/stats/apnic/delegated-apnic-latest>). `cn-routefeed` might be useful if you are trying to set up special routing for IPv4 routes that are based in China.

If APNIC changed the URL of delegation information file in the future, you could change that URL on line 5 of `fetcher.cc`.

### Installation

`cn-routefeed` need the following build dependencies:

- g++ (or any other c++ compiler)
- [libbgp](https://github.com/Nat-Lab/libbgp) 
- libcurl
- cmake (optional)

For libbgp, please follow the installation instruction on the [project page](https://github.com/Nat-Lab/libbgp). For other build dependencies, you can install them on a Debian-based Linux distribution with the following `apt` command:

```
# apt install g++ libcurl4-nss-dev cmake
```

Once you have the dependencies installed, use the following commands to build `routefeed`:

```
$ git clone https://github.com/nat-lab/cn-routefeed
$ cd cn-routefeed
$ mkdir build
$ cd build
$ cmake ../
$ make
```

Or, if you are not into the `cmake` kind of thing, you can build the project with:

```
$ g++ -lbgp -lcurl -lpthread *.cc -oroutefeed 
```

It should work on most of the cases.

### Usage

`cn-routefeed` is a simple tool. The command-line help is pretty self-explanatory:

```
usage: routefeed [-l HOST] [-p PORT] [-t INTERVAL] -a ASN -i BGP_ID -n NEXTHOP

cn-routefeed is a BGP speaker that feeds all China IPv4 delegations to peer. 
Delegation information are fetch from APNIC.

required arguments:
  -a ASN       Local ASN of the BGP speaker.
  -i BGP_ID    BGP ID (Router ID) of the BGP speaker.
  -n NEXTHOP   Nexthop to use when sending routes.

optional arguments:
  -l HOST      IP address to bind the BGP speaker on. (default: 0.0.0.0)
  -p PORT      TCP port number to bind the BGP speaker on. (default: 179)
  -t INTERVAL  Time in second to wait between fetching update from APNIC.
               (default: 86400)
```

For example, you can start a feeder with the following command:

```
# routefeed -a 65000 -i 172.16.0.1 -n 172.16.0.1
```

This command starts a BGP speaker on `0.0.0.0:179`, with ASN 65000. It accepts peer with any ASN and will feed the peer with routes using `172.16.0.1` as nexthop.

### License

UNLICENSE