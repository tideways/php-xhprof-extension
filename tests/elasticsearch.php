<?php

namespace Elasticsearch\Connections {

    class Connection {
        public function performRequest($method, $uri, $params = null, $body = null, $options = array(), $transport = null)
        {
        }
    }
}

namespace Elasticsearch\Endpoints {
    abstract class AbstractEndpoint {
        public function resultOrFuture($result) {
        }
    }

    class SomeEndpoint extends AbstractEndpoint {
    }

    class AnotherEndpoint extends AbstractEndpoint {
    }
}
