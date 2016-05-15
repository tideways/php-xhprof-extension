<?php

class Mage
{
    static public function dispatchEvent($eventName, $params)
    {
        usleep(100);
    }
}

class Enlight_Event_EventManager
{
    public function filter($event, $value, $args) {usleep(100);}
    public function notify($event, $args) {usleep(100);}
    public function notifyUntil($event, $args) {usleep(100);}
}

function do_action($name, $params) {
    usleep(100);
}
function apply_filters($name, $params) {
    usleep(100);
}
function drupal_alter($name, $args) {
    usleep(100);
}

class CakeEventManager {
    public function dispatch($event) {
        if (is_string($event)) {
            $event = new CakeEvent($event);
        }
    }
}

class CakeEvent
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
