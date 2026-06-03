--TEST--
Stream hook: use case
--FILE--
<?php

class Scheduler
{
    private $readFds = [];
    private $writeFds = [];
    private $readFibers = [];
    private $writeFibers = [];

    private $ready = [];

    public function run($main)
    {
        $this->ready[] = new Fiber($main);

        while ($this->ready !== [] || $this->readFds !== [] || $this->writeFds !== []) {
            $this->runReadyFibers();
            $this->pollFds();
        }
    }

    public function runReadyFibers()
    {
        while ($this->ready !== []) {
            $fiber = array_shift($this->ready);
            $fiber->isSuspended() ? $fiber->resume() : $fiber->start();
        }
    }

    public function pollFds()
    {
        if ($this->readFds !== [] || $this->writeFds !== []) {
            $read = $this->readFds;
            $write = $this->writeFds;
            $except = [];
            stream_select($read, $write, $except, null);
            foreach ($read as $fd) {
                $id = (int)$fd;
                array_push($this->ready, ...$this->readFibers[$id]);
                unset($this->readFds[$id]);
                unset($this->readFibers[$id]);
            }
            foreach ($write as $fd) {
                $id = (int)$fd;
                array_push($this->ready, ...$this->writeFibers[$id]);
                unset($this->writeFds[$id]);
                unset($this->writeFibers[$id]);
            }
        }
    }

    public function waitRead($fd) {
        $id = (int)$fd;
        $this->readFds[$id] = $fd;
        $this->readFibers[$id][] = Fiber::getCurrent();
        Fiber::suspend();
    }

    public function waitWrite($fd) {
        $id = (int)$fd;
        $this->writeFds[$id] = $fd;
        $this->writeFibers[$id][] = Fiber::getCurrent();
        Fiber::suspend();
    }

    public function go(callable $fn) {
        $this->ready[] = new Fiber($fn);
        $this->ready[] = Fiber::getCurrent();
        Fiber::suspend();
    }
}

$scheduler = new Scheduler();

stream_set_hook(function ($fd, StreamOperation $operation) use ($scheduler) {
    switch ($operation) {
        case StreamOperation::Read:
            $scheduler->waitRead($fd);
            break;
        case StreamOperation::Write:
            $scheduler->waitWrite($fd);
            break;
    }
});

function go($fn) {
    global $scheduler;
    $scheduler->go($fn);
}

$scheduler->run(function () {
    $server = stream_socket_server('tcp://localhost:0');
    $socket_name = stream_socket_get_name($server, false);
    if (!preg_match('/:(\d+)$/', $socket_name, $m)) {
        die("Could not extract port from '$socket_name'");
    }
    $port = $m[1];

    go(function () use ($server) {
        $client = stream_socket_accept($server);
        go(function () use ($client) {
            $headers = [];
            while (!feof($client)) {
                $line = fgets($client);
                if ($line === false) {
                    break;
                }
                if ($line === "\r\n") {
                    break;
                }
                $headers[] = $line;
            }
            foreach ($headers as $header) {
                echo "> " . trim($header) . "\n";
            }
            fwrite($client, "HTTP/1.0 200 OK\r\n");
            fwrite($client, "\r\n");
            fwrite($client, "Hello world!\n");
        });
    });

    go(function () use ($port) {
        $fd = stream_socket_client("tcp://localhost:$port");
        fwrite($fd, "GET / HTTP/1.0\r\n");
        fwrite($fd, "Host: localhost\r\n");
        fwrite($fd, "\r\n");
        while (!feof($fd)) {
            echo "< " . trim(fgets($fd)) . "\n";
        }
        fclose($fd);
    });
});

?>
--EXPECT--
> GET / HTTP/1.0
> Host: localhost
< HTTP/1.0 200 OK
<
< Hello world!
