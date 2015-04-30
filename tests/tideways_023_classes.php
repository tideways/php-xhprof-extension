<?php

namespace Symfony\Component\HttpKernel\Controller {
    class ControllerResolver
    {
        public function createController($name)
        {
        }
    }
}

namespace Zend\MVC\Controller {
    class ControllerManager
    {
        public function get($name, $options = array(), $usePeeringServiceManagers = false)
        {
        }
    }
}

namespace Illuminate\Routing {
    abstract class Controller {
        public function callAction($method, $parameters) {
        }
    }
}

namespace CachetHQ\Cachet\Http\Controllers {
    class RssController extends \Illuminate\Routing\Controller {
        public function indexAction() {}
    }
}
