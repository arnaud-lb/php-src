--TEST--
Stream hook: closed during read hook
--FILE--
<?php

use Io\Hooks\{Hooks, PollInfo, PollResult};

$server = stream_socket_server('tcp://127.0.0.1:0');
$addr = stream_socket_get_name($server, false);

$client = stream_socket_client("tcp://$addr");
$conn = stream_socket_accept($server);
fwrite($conn, "<?php\n");
fclose($conn);
fclose($server);

class CloseOnceHooks implements Hooks {
    public function poll(PollInfo $info): PollResult {
        fclose($info->handle->getStream());
        Io\Hooks\set_hooks(null);
        $result = new PollResult();
        $result->handle = $info->handle;
        $result->events = $info->events;
        $result->timeout = false;
        return $result;
    }
    public function poll_multi(?int $timeout_ms, PollInfo ...$info): array { throw new \Exception("poll_multi not implemented"); }
}

Io\Hooks\set_hooks(new CloseOnceHooks());
var_dump(fgets($client));
?>
--EXPECTF--
Warning: fclose(): cannot close the provided stream, as it must not be manually closed in %s on line %d
string(6) "<?php
"
