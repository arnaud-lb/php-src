--TEST--
Io\Poll: onWatcherRemoved fires on stream GC but not on manual Watcher::remove()
--FILE--
<?php
require_once __DIR__ . '/poll.inc';

[$r, $w] = pt_new_socket_pair();

// --- Part 1: manual remove does NOT fire the callback ---
$ctx = pt_new_stream_poll();
$ctx->onWatcherRemoved(function (Io\Poll\Watcher $w) {
    echo "removed: " . $w->getData() . "\n";
});

$handle = StreamPollWeakHandle::create($r);
$watcher = $ctx->add($handle, [Io\Poll\Event::Read], 'manual');
$watcher->remove();  // no callback expected
echo "after manual remove\n";
unset($watcher, $handle);

// --- Part 2: stream GC fires the callback ---
$handle = StreamPollWeakHandle::create($r);
$ctx->add($handle, [Io\Poll\Event::Read], 'stream-gc');
unset($handle);

fclose($r);  // notifies context -> onWatcherRemoved
fclose($w);

echo "done\n";
?>
--EXPECT--
after manual remove
removed: stream-gc
done
