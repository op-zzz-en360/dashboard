(function(angular) {
  'use strict';
angular.module('httpExample', []).config(function($interpolateProvider) {
  $interpolateProvider.startSymbol('{[{');
  $interpolateProvider.endSymbol('}]}');
})
        .controller('FetchController', ['$scope', '$http', '$templateCache',
            function($scope, $http, $templateCache) {
            $scope.method = 'GET';
            $scope.url = 'config';
            $scope.newCamera;

            $scope.delCamera = function(camindex) {
                $scope.data.cameras.splice(camindex, 1);
            };

            $scope.addCamera = function() {
                $scope.data.cameras.push({name:$scope.newCamera.name, ipaddr:$scope.newCamera.ipaddr});
                $scope.newCamera.name = "";
                $scope.newCamera.ipaddr = "";
            };

            $scope.fetch = function() {
                $scope.code = null;
                $scope.response = null;
                console.log("sending get");
                $http({method: $scope.method, url: $scope.url, cache: $templateCache}).
                then(function(response) {
                    console.log("good get");
                    console.log(response);
                    $scope.status = response.statusText;
                    $scope.data = response.data;
                }, function(response) {
                    console.log("bad get");
                    $scope.data = response.data || "Request failed";
                    $scope.status = response.statusText;
                });
            };
            $scope.post_config  = function() {
                console.log("saving...");
                console.log(JSON.stringify($scope.data));
                var arrayLength = $scope.data.cameras.length;
                var dataArray = [];
                for (var i = 0; i < arrayLength; i++) {
                    dataArray[i] = {
                        'name' : $scope.data.cameras[i]['name'],
                        'ipaddr' : $scope.data.cameras[i]['ipaddr']
                    };
                }
                var dataObject = {'cameras': dataArray};
                console.log(JSON.stringify(dataObject));
                var data = JSON.stringify(dataObject);
                $http.post("/config", data).success(function(data, status) {
                    console.log(data);
                })
            }
            $scope.fetch();
            }]);
})(window.angular);
