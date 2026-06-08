--TEST--
Stream hook: use case
--FILE--
<?php

use Io\Hooks\{Hooks, PollInfo, PollResult};
use Io\Poll\{Context, Event};

class Scheduler implements Hooks
{
    private Context $pollContext;
    private array $fds = [];
    private array $ready = [];
    private array $fiberWatchers = [];   // fiberId => Watcher[]
    private array $fiberDeadlines = [];  // fiberId => [deadline_ns, fiber, PollInfo]

    public function __construct()
    {
        $this->pollContext = new Context();
    }

    public function run($main): void
    {
        $this->ready[] = [new Fiber($main), null];

        while ($this->ready !== [] || $this->fds !== [] || $this->fiberDeadlines !== []) {
            $this->runReadyFibers();
            $this->pollFds();
        }
    }

    public function runReadyFibers(): void
    {
        while ($this->ready !== []) {
            [$fiber, $resumeValue] = array_shift($this->ready);
            $fiber->isSuspended() ? $fiber->resume($resumeValue) : $fiber->start();
        }
    }

    public function pollFds(): void
    {
        if ($this->fds === [] && $this->fiberDeadlines === []) {
            return;
        }

        // Compute wait() timeout from the nearest deadline
        $timeoutSec = null;
        $timeoutUsec = 0;
        $now = hrtime(true);
        foreach ($this->fiberDeadlines as [$deadline]) {
            $remaining = $deadline - $now;
            if ($remaining <= 0) {
                $timeoutSec = 0;
                $timeoutUsec = 0;
                break;
            }
            $remainingUsec = intdiv($remaining, 1_000);
            if ($timeoutSec === null || $remainingUsec < $timeoutSec * 1_000_000 + $timeoutUsec) {
                $timeoutSec = intdiv($remainingUsec, 1_000_000);
                $timeoutUsec = $remainingUsec % 1_000_000;
            }
        }

        $watchers = $this->pollContext->wait($timeoutSec, $timeoutUsec);

        // Handle ready handles
        foreach ($watchers as $watcher) {
            [$fiber] = $watcher->getData();
            $fiberId = spl_object_id($fiber);

            if (!isset($this->fiberWatchers[$fiberId])) {
                continue;
            }

            $this->removeAllFiberWatchers($fiberId);
            unset($this->fiberDeadlines[$fiberId]);

            $result = new PollResult();
            $result->handle = $watcher->getHandle();
            $result->events = $watcher->getTriggeredEvents();
            $result->timeout = false;

            $this->ready[] = [$fiber, $result];
        }

        // Handle expired deadlines
        $now = hrtime(true);
        foreach ($this->fiberDeadlines as $fiberId => [$deadline, $fiber, $info]) {
            if ($deadline > $now) {
                continue;
            }

            $this->removeAllFiberWatchers($fiberId);
            unset($this->fiberDeadlines[$fiberId]);

            $result = new PollResult();
            $result->handle = $info->handle;
            $result->events = $info->events;
            $result->timeout = true;

            $this->ready[] = [$fiber, $result];
        }
    }

    private function removeAllFiberWatchers(int $fiberId): void
    {
        foreach ($this->fiberWatchers[$fiberId] as $w) {
            unset($this->fds[(int)$w->getHandle()->getStream()]);
            $w->remove();
        }
        unset($this->fiberWatchers[$fiberId]);
    }

    public function poll(PollInfo $info): PollResult
    {
        $fiber = Fiber::getCurrent();
        $fiberId = spl_object_id($fiber);

        if ($info->timeout_ms >= 0) {
            $deadline = hrtime(true) + $info->timeout_ms * 1_000_000;
            $this->fiberDeadlines[$fiberId] = [$deadline, $fiber, $info];
        }

        $id = (int)$info->handle->getStream();
        if (isset($this->fds[$id])) {
            throw new \Exception("Handle already registered");
        }
        $this->fds[$id] = $id;
        $this->fiberWatchers[$fiberId] = [$this->pollContext->add($info->handle, $info->events, [$fiber])];

        /** @var PollResult $result */
        $result = Fiber::suspend();
        return $result;
    }

    public function poll_multi(?int $timeout_ms, PollInfo ...$infos): array
    {
        throw new \Exception("poll_multi not implemented");
    }

    public function go(callable $fn): void
    {
        $this->ready[] = [new Fiber($fn), null];
        $this->ready[] = [Fiber::getCurrent(), null];
        Fiber::suspend();
    }
}

$scheduler = new Scheduler();

Io\Hooks\set_hooks($scheduler);

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
