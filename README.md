Network
-------
- For incoming connections, ensure any stateful packetfilter on the system
  has plenty of headroom regarding its table sizes:
  - iptables conntrack_max/pf states
    - todays systems have enough memory
    - for iptables make sure conntrack_max and hashtable size match
      - contrack_max = hashtable * 8 (if conntrack_max is set manually,
        see [net/netfilter/nf_conntrack_core.c](https://github.com/torvalds/linux/blob/6363b3f3ac5be096d08c8c504128befa0c033529/net/netfilter/nf_conntrack_core.c#L2003-L2042))
      - e.g. 1048576 = 131072 * 8
  - If there are spikes in the amount of incoming connections,
    have larger accept queues per listener:
    - kern.ipc.soacceptqueue=32768 (FreeBSD)
    - net.core.somaxconn=32768 (Linux)
- For outgoing connections, ensure there are plenty of free local TCP
  ports available:
  - net.inet.ip.portrange.first=1024 (FreeBSD)
  - net.inet.ip.portrange.last=65535 (FreeBSD)
  - net.ipv4.ip_local_port_range=1024 65535 (Linux)

SSL
---
- Handshake time directly affects application response latency and CPU
  cycles consumption
  - Use TLS 1.3
    - three step handshake instead of four steps
      ```
      TLS 1.2 (full handshake):         TLS 1.3 (full handshake):
         Client               Server    Client                  Server
      ------------------------------    ------------------------------
      0) ---> TCP SYN           --->    0) ---> TCP SYN           --->
      0) <--- TCP SYN ACK       <---    0) <--- TCP SYN ACK       <---
      0) ---> TCP ACK           --->    0) ---> TCP ACK           --->
      1) ---> Client Hello      --->    1) ---> Client Hello,
      2) <--- Server Hello,                     Key Share         --->
              Certificate,              2) <--- Server Hello,
              Server Hello Done <---            Key Share,
      3) ---> Cipher Key Exchange,              Certificate,
              Change Cipher Spec,               Certificate Verify,
              Finished          --->            Finished          <---
      4) <--- Change Cipher Spec,       3) ---> Finished,
              Finished          <---            HTTP Request      --->
      5) ---> HTTP Request      --->    4) <--- HTTP Response     <---
      6) <--- HTTP Response     <---
      ```
  - Use TLS session resumption
    - default setting in newer HAproxy
    - Nginx: ssl_session_cache, ssl_session_timeout
  - If TLS session resumption is useful in reducing latency, consider
    TCP Fast Open ([RfC 7413](https://datatracker.ietf.org/doc/rfc7413/))
    - reduces TCP handshake to two steps on reconnect
    - server side support: Linux, FreeBSD,
      [HAproxy](https://github.com/haproxy/haproxy/blob/153659f1ae69a1741109fcb95cac2c7d64f99a29/src/proto_tcp.c#L1041-L1065), Nginx
  - Use 2048 bit RSA key pair instead of 4096 bit
    - if security is of lesser and time of higher importance
      (unlike online payments for instance)
- Announce Server Preferred Ciphers
  - omit slow ciphers, 3DES
  - have a look at `openssl speed`
  - consider using different compiler optimisation (-O3, default setting
    for OpenSSL)
  - consider using AES-NI instructions in your CPU
    - Intel E7-4830 v4 has them (check with `openssl version -a`)

On multi-processor systems
--------------------------

  - Bind SSL-proxy software to cpu cores
    - have fewer processor crosscalls
    - have fewer NUMA remote memory accesses
      - quad socket Intel E7-4830 v4 is a NUMA system
      - memory latencies:
        - socket local: 135 ns
        - remote: 194-202 ns
          ([source](https://software.intel.com/en-us/blogs/2014/01/28/memory-latencies-on-intel-xeon-processor-e5-4600-and-e7-4800-product-families))

  - HAproxy:
    - use option `nbproc 52` or `nbproc 104` depending if Hyperthreading
      is used, leaving 4 cores (8 threads) for other processes
      ```
      cpu-map 1 4
      cpu-map 2 5
      cpu-map 3 6
      [...]
      cpu-map 50 55
      ```
    - maybe have four HAproxy master processes, one on each socket,
      with their child processes on the CPU cores of that socket

    - `balance static-rr` might save some CPU cycles

    - test if changes actually yield different performance
      (best testing is always in production, start removing nodes,
       watch metrics in Grafana or similar, stop once you reach
       obvious limits, analyze, improve)

Metrics
-------

At system level:
- prometheus node_exporter or collectd
  - CPU usage and saturation
    - sys, intr, user, nice, idle, iowait (Munin style)
  - iptables conntrack/pf states
  - network established/time_wait/fin_wait connections
  - number of open files (correlates with network connections)
  - network bandwidth
  - network packets (for even workload correlates with network bandwith)
  - context switches/interrupts (correlates with network packets)
  - storage latency for logfiles
  - maybe have a CPU usage graph showing distribution among cores
  - maybe processor cross-calls (mpstat "xcal")
  - if your workload has load spikes, consider sampling metrics at
    shorter intervals (1s), calcuate maximum, 99% percentile, median,
    and present that data to prometheus/collectd (prior aggregation)

At application level:
  - Curl your application from within your network, from your SSL-gateways,
    from the outside world:
    - `time_connect`
    - `time_pretransfer`
    - `time_total`
    - see [`curl-stats.c`](https://github.com/k0ffee/ssl-offload/blob/master/curl-stats.c)
      ```
      % ./curl-stats
      Total download seconds: 2.025787
      Total pretransfer seconds: 1.453561
      Name lookup seconds: 0.005250
      Connect time seconds: 0.788709
      ```
    - use this with an IP address, circumventing DNS latency,
      and watch connect time metric over time

Have some nice and sleek looking graphical frontend
that people enjoy using *before* the trouble starts.

Grafana is slow, but can do it.

In general, we need three types of monitoring:
  - long term statistics
    - "How did it look like one year ago?"
    - "Do we need to add more machines in a month?"
  - current statistics
    - "Did my application deploy go well or wrong?"
    - This needs to be useable for developers
  - immediate metrics
    - "What is going on right now?"
    - `top`, `vmstat`, `iostat`, `mpstat`, `perf`, `dtrace`,
      `tail -F logfile`, etc.
    - read Brendan Gregg's [blog](http://brendangregg.com/blog/index.html)

HAproxy logs:
  - logging to local file:
    - expect large files
    - don't keep old files
    - skip compression during logrotation
    - options:
      - for less data: `option dontlog-normal`
      - for even lesser data: `http-request set-log-level silent if ...`
      - custom log-format:
        - `%Th` - connection handshake time (SSL)
          - "100% of handshakes above 40ms are from the US."
        - `%Ti` - idle time before the HTTP request
          - if handshake time is zero but idle time is high,
            we are reusing connections
        - if this is written to logfiles, have some monitoring plugin
          sampling from this logfile (`tail`) and calculate maximum,
          99% percentile, median - it would be interesting to see
          how many transactions we have and how many required a fresh
          SSL handshake
    - Syslog: `rsyslog` can do random log sampling, might miss
      rare interesting events
    - central logging:
      - HAproxy → /var/log/haproxy.log → Filebeat → Elasticsearch → Kibana
      - HAproxy → Syslog (UDP) → Logstash → Elasticsearch → Kibana
