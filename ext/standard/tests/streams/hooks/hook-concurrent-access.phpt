--TEST--
Stream hook: concurrent stream access is rejected
--FILE--
<?php

use Io\Hooks\{Hooks, PollInfo, PollResult};

$server = stream_socket_server('tcp://127.0.0.1:0');
$addr = stream_socket_get_name($server, false);
$client = stream_socket_client("tcp://$addr");

class ConcurrentHook implements Hooks {
    public function poll(PollInfo $info): PollResult {
        try {
            fgets($info->handle->getStream());
        } catch (Error $e) {
            echo $e->getMessage() . "\n";
        }
        $result = new PollResult();
        $result->handle = $info->handle;
        $result->events = $info->events;
        $result->timeout = false;
        return $result;
    }
    public function pollMulti(?int $timeout_ms, PollInfo ...$info): ?PollResult {
        throw new \Exception("pollMulti not implemented");
    }
    public function sleep(int $seconds, int $nanoseconds): void { throw new \Exception("sleep not implemented"); }
}

Io\Hooks\set_hooks(new ConcurrentHook());
var_dump(fgets($client));
?>
--EXPECT--
Concurrent access to a stream
bool(false)
