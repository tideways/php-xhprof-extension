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
        }
    }
}

namespace Doctrine\Common {
    class EventManager
    {
        public function dispatchEvent($name, $event)
        {
        }
    }
}
