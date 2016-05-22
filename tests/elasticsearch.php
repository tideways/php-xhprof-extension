<?php

namespace Elasticsearch\Connections {

    class Connection {
        public function performRequest($method, $uri, $params = null, $body = null, $options = [], $transport = null)
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
