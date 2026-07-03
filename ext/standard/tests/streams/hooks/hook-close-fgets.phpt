--TEST--
Stream hook: fclose() in poll() throws concurrent access error
--FILE--
<?php

use Io\Hooks\{Hooks, PollInfo, PollResult};

$server = stream_socket_server('tcp://127.0.0.1:0');
$addr = stream_socket_get_name($server, false);
$client = stream_socket_client("tcp://$addr");

class CloseOnceHooks implements Hooks {
    public function poll(PollInfo $info): PollResult {
        fclose($info->handle->getStream());
        $result = new PollResult();
        $result->handle = $info->handle;
        $result->events = $info->events;
        $result->timeout = false;
        return $result;
    }
    public function pollMulti(?int $timeout_ms, PollInfo ...$info): ?PollResult { throw new \Exception("pollMulti not implemented"); }
    public function sleep(int $seconds, int $nanoseconds): void { throw new \Exception("sleep not implemented"); }
}

Io\Hooks\set_hooks(new CloseOnceHooks());
try {
    fgets($client);
} catch (Error $e) {
    echo $e->getMessage() . "\n";
}
?>
--EXPECT--
Concurrent access to a stream
