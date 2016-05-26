<?php

namespace Symfony\Component\EventDispatcher {
    class EventDispatcher
    {
        public function dispatch($eventName, Event $event)
        {
            $this->doDispatch(array(), $eventName, $event);
        }

        protected function doDispatch(array $listeners, $eventName, $event)
        {
            usleep(100);
        }
    }

    class Event
    {
    }
}

namespace Zend\EventManager {
    class EventManager
    {
        public function trigger($event, $context, $params)
        {
            usleep(100);
        }
    }
}

namespace Doctrine\Common {
    class EventManager
    {
        public function dispatchEvent($name, $event)
        {
            usleep(100);
        }
    }
}

namespace TYPO3\Flow\SignalSlot {
    class Dispatcher {
        public function dispatch($signalClassName, $signalName) {
        }
    }
}

namespace Cake\Event {
    class EventManager {
        public function dispatch($event) {
            if (is_string($event)) {
                $event = new Event($event);
            }
        }
    }

    class Event
    {
        private $name;

        public function __construct($name)
        {
            $this->name = $name;
        }

        public function name()
        {
            return $this->name;
        }
    }
}
