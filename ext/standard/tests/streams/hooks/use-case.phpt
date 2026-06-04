--TEST--
Stream hook: use case
--FILE--
<?php

use Io\Poll\Context;

class Scheduler
{
    private $pollContext;
    private $fds = [];
    private $ready = [];

    public function __construct()
    {
        $this->pollContext = new Context();
    }

    public function run($main)
    {
        $this->ready[] = new Fiber($main);

        while ($this->ready !== [] || $this->fds !== []) {
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
        if ($this->fds !== []) {

            $watchers = $this->pollContext->wait();

            foreach ($watchers as $watcher) {
                $id = (int)$watcher->getHandle()->getStream();
                unset($this->fds[$id]);

                $this->ready[] = $watcher->getData();

                $watcher->remove();
            }
        }
    }

    public function pollFd($fd, array $events)
    {
        $id = (int)$fd;
        if (isset($this->fds[$id])) {
            throw new Exception();
        }

        $this->fds[$id] = $id;
        $this->pollContext->add(new StreamPollHandle($fd), $events, Fiber::getCurrent());

        Fiber::suspend();

        return StreamHookResult::Ready;
    }

    public function go(callable $fn) {
        $this->ready[] = new Fiber($fn);
        $this->ready[] = Fiber::getCurrent();
        Fiber::suspend();
    }
}

$scheduler = new Scheduler();

stream_set_hook($scheduler->pollFd(...));

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
            fclose($client);
        });
    });

    go(function () use ($port) {
        $fd = stream_socket_client("tcp://localhost:$port");
        fwrite($fd, "GET / HTTP/1.0\r\n");
        fwrite($fd, "Host: localhost\r\n");
        fwrite($fd, "\r\n");
        while (!feof($fd)) {
            echo trim("< " . fgets($fd)) . "\n";
        }
    });
});

?>
--EXPECT--
> GET / HTTP/1.0
> Host: localhost
< HTTP/1.0 200 OK
<
< Hello world!
