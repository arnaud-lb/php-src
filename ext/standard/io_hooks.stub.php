<?php

/** @generate-class-entries */

namespace Io\Hooks {

    use Io\Poll\Handle;

    final class PollInfo {
        public Handle $handle;
        /* @var Io\Poll\Event[] */
        public array $events;
        public int $timeout_ms;
    }

    enum PollResult {
        case Error;
        case Timeout;
        case Ready;
    }

    interface Hooks {
        public function poll(PollInfo $info): PollResult;
    }

    function set_hooks(?Hooks $hooks): ?Hooks {}
}
