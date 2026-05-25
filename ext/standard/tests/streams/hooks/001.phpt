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
    [$client, $server] = stream_socket_pair(STREAM_PF_UNIX, STREAM_SOCK_STREAM, STREAM_IPPROTO_IP);

    go(function () use ($client) {
        fwrite($client, "GET / HTTP/1.0\r\n");
        fwrite($client, "Host: localhost\r\n");
        fwrite($client, "\r\n");
        while (!feof($client)) {
            echo "< " . trim(fgets($client)) . "\n";
        }
        fclose($client);
    });

    go(function () use ($server) {
        $headers = [];
        while (!feof($server)) {
            $line = fgets($server);
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
        fwrite($server, "Hello world!\n");
    });
});

?>
--EXPECT--
> GET / HTTP/1.0
> Host: localhost
< Hello world!
