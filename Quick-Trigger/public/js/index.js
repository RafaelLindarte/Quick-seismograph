var Toast = Swal.mixin({
  toast: true,
  showConfirmButton: false,
  timer: 2000
});
var payload;
var nodeWS;
var triggerWS;
var nodeCommand;
/************************************************
*
*                 HTTP REQUESTS                 *
*
*************************************************/
const sendHttpRequest = (method, url, data) => {
  return fetch(url, {
    method: method,
    body: JSON.stringify(data),
    headers: data ? { 'Content-Type': 'application/json' } : {}
  }).then(response => {
    // console.log(response);
    if (response.status >= 400) {
      // !response.ok
      return response.json().then(errResData => {
        const error = new Error('Something went wrong!');
        error.data = errResData;
        throw error;
      });
    }
    return response;
  });
};

/************************************************
*
*                 TESTING PANE                  *
*
*************************************************/

var data = [];
var dataset;
var totalPoints = 1251;
var updateInterval = 10;
var now = new Date().getTime();

var i=0;
function initData() {
    data.shift();
    while (data.length < totalPoints) {
      y=0;
      data.push(y)
    }
    // Zip the generated y values with the x values
    var res = []
    for (var i = 0; i < data.length; ++i) {
      res.push([i, data[i]])
    }
    return res
}

function GetData() {
    data.shift();
    while (data.length < totalPoints) {
        if(i>=totalPoints){
          i=0;
        }
        data.push([ now += updateInterval, data[i]]);
        ++i;
    }
}

var options = {
    grid: {
        borderColor: '#f3f3f3',
        borderWidth: 1,
        tickColor: '#f3f3f3'
    },
    series: {
        color: '#3c8dbc',
        lines: {
        lineWidth: 2,
        show: true,
        fill: false,
        },
    },
    yaxis: {
        min: 0,
        max: 1,
        show: true
    },
    xaxis:{
        mode: "time",
        tickSize: [200, "second"],
        axisLabel: "Time",
        axisLabelUseCanvas: true,
        axisLabelFontSizePixels: 12,
        axisLabelFontFamily: 'Verdana, Arial',
        axisLabelPadding: 10
    }
}

initData();

dataset = [{data}];

$.plot($("#interactive"), dataset, options);

function update() {
  console.log("update");
    console.log(data);
    GetData();
    $.plot($("#interactive"), dataset, options)
    // setTimeout(update, updateInterval);
}
// update();
const getNodes = document.getElementById('scanButton');
const connectNode = document.getElementById('connectButton');
const disconnectNode = document.createElement("button");
var disconnectButtonText = document.createElement("h4")
disconnectNode.setAttribute("class","btn btn-block btn-danger");
disconnectNode.setAttribute("id","disconnectButton");
disconnectButtonText.innerText = "Disconnect";
disconnectNode.appendChild(disconnectButtonText);

//subsuelo3d.quick
const disconnectFromNode = async() => {
  var ssid = $( "#availableList option:selected" ).text();
  try {
    nodeWS.close()
    var response = await sendHttpRequest('GET', 'http://124.213.16.29:80/api/v1/disconnectNode');
    if(response.status == 500){
      Toast.fire({
        icon: 'error',
        title: 'Desconexión ha fallado.'
      })
      return;
    }
    Toast.fire({
      icon: 'success',
      title: `${ssid} desconectado`
    })
    disconnectNode.remove();
    document.getElementById("swapButtons").append(connectNode);
  } catch (error) {
    Toast.fire({
      icon: 'error',
      title: 'Perdida de conexión'
    })
  }
};

const connectToNode = async() => {
  var ssid = $( "#deviceList option:selected" ).text();
  if (!(typeof (ssid) === 'string' && ['', 'null', 'undefined'].indexOf(ssid) == -1))
  {
    Toast.fire({
      icon: 'warning',
      title: 'Selecione un nodo para la conexión.'
    })
    return;
  }
  try {
    var response = await sendHttpRequest('POST', 'http://124.213.16.29:80/api/v1/connectNode', {ssid:ssid.trim()});
    nodeWS = new WebSocket("ws://124.213.16.29:94/ws/connectNode");
    nodeWS.onopen = nodeOnOpen;
    nodeWS.onclose = nodeOnClose;
    nodeWS.onerror = nodeOnError;
    nodeWS.onmessage = nodeOnMessage;
    nodeCommand = "test";
    if(response.status == 500){
      // nodeWS.close()
      Toast.fire({
        icon: 'error',
        title: 'Conexión ha fallado.'
      })
      return;
    }
    Toast.fire({
      icon: 'success',
      title: `${ssid} conectado`
    })
    connectNode.remove();
    document.getElementById("swapButtons").append(disconnectNode);
    } catch (error) {
      Toast.fire({
        icon: 'error',
        title: 'Perdida de conexión'
      })
    }
};

const scanNodes = async() => {
  try {
    var response = await sendHttpRequest('GET', 'http://124.213.16.29:80/api/v1/scanNodes');
    const {nodes} = await response.json();
    if(response.status == 500){
      Toast.fire({
        icon: 'error',
        title: 'Scan Failed'
      })
      return;
    }
    $('.modal-backdrop').hide();
    $('.modal').hide();
    var nodeList = document.getElementById("deviceList").length
    // console.log(nodeList);
    if(nodeList != 0){
      for (let index = 0; index < nodeList; index++) {
        document.getElementById("deviceList").options.remove(0);
      }
    }
    else{
      $("#cardList").removeClass("collapsed-card");
    }
    nodes.forEach(element => {
      $("#deviceList").append(`<option> ${element} </option>`);

    });
    for (let index = 0; index < document.getElementById("deviceList").length ; index++) {
      document.getElementById("deviceList").children.item(index).onclick = ()=>{
        // console.log(document.getElementById("deviceList").children.item(index).innerText);
        document.getElementById("nodeLabel").innerHTML = "Selected: \n"+document.getElementById("deviceList").children.item(index).innerText;
      }
      document.getElementById("deviceList").children.item(index).onselect = ()=>{
        // console.log(document.getElementById("deviceList").children.item(index).innerText);
        document.getElementById("nodeLabel").innerHTML = "Selected: \n"+document.getElementById("deviceList").children.item(index).innerText;
      }
    }
    Toast.fire({
      icon: 'success',
      title: 'Scan realizado.'
    })
  } catch (error) {
    Toast.fire({
      icon: 'error',
      title: 'Perdida de conexión'
    })
  }
};

getNodes.addEventListener('click', scanNodes);
connectNode.addEventListener('click', connectToNode);
disconnectNode.addEventListener('click',disconnectFromNode)

/************************************************
*
*                 ACQUIRING PANE                *
*
*************************************************/
var triggerFile = {};
var numShots=1;
var numShotPoints=0;
const activeMeasurementRadio = document.getElementById("ActiveMeasurement");
const remiRadio = document.getElementById("REMI");
const listRecordLength = document.getElementById("recordLength");
const listNodeQuatity = document.getElementById("nodeQuantity");
const projectName = document.getElementById("Name");
const saveFormButton = document.getElementById("saveButton");
const startMeasurementButton = document.getElementById("startButton");

const stopMeasurementButton = document.createElement("button");
var stopMeasurementButtonText = document.createElement("h4");
stopMeasurementButton.setAttribute("class","btn btn-danger float-right");
stopMeasurementButton.setAttribute("id","stopButton");
stopMeasurementButtonText.innerText = "Stop";
stopMeasurementButton.appendChild(stopMeasurementButtonText);

var shotPointButton = document.getElementById("shotPoint");
shotPointButton.disabled = true;

const finishShotPointButton = document.createElement("button");
var finishShotPointButtonText = document.createElement("h4");
finishShotPointButton.setAttribute("class","btn btn-block btn-primary");
finishShotPointButton.setAttribute("id","stopButton");
finishShotPointButtonText.innerText = "Finnish ShotPoint";
finishShotPointButton.appendChild(finishShotPointButtonText);
activeMeasurementRadio.onclick =()=>{

  var optionsNumber = listRecordLength.length;
  if(optionsNumber != 0){
    for (let index = 0; index < optionsNumber; index++) {
      listRecordLength.options.remove(0);
    }
  }
  for (let index = 0; index < 8; index++) {
    var option = document.createElement("option");
    listRecordLength.append(option);
    listRecordLength.children[index].text = `${(index+1)*0.25}`;
  }
  for (let index = 0; index < 10; index++) {
    var option = document.createElement("option");
    listNodeQuatity.append(option);
    listNodeQuatity.children[index].text = `${(index+1)}`;

  }
}
remiRadio.onclick =()=>{

  var optionsNumber = listRecordLength.length;
  if(optionsNumber != 0){
    for (let index = 0; index < optionsNumber; index++) {
      listRecordLength.options.remove(0);
    }
  }
  for (let index = 0; index < 20; index++) {
    var option = document.createElement("option");
    listRecordLength.append(option);
    listRecordLength.children[index].text = `${(index+1)*1}`;
  }
  for (let index = 0; index < 10; index++) {
    var option = document.createElement("option");
    listNodeQuatity.append(option);
    listNodeQuatity.children[index].text = `${(index+1)}`;

  }
}

const saveConfig = async() => {
  if( (['',""].indexOf(projectName.value) != -1) &&
      (['',""].indexOf(listRecordLength.value) != -1) &&
      (['',""].indexOf(listNodeQuatity.value) != -1))
  {
    return;
  }
  document.getElementById("nameLabel").textContent = document.getElementById("Name").value;
  document.getElementById("recordLengthLabel").textContent = document.getElementById("recordLength").value;
  document.getElementById("nodeQuantityLabel").textContent = document.getElementById("nodeQuantity").value;
  document.getElementById("shotPointLabel").textContent = 0;
  document.getElementById("shotLabel").textContent = 0;
  triggerFile["config"]={
    recordLength: document.getElementById("recordLength").value,
    nodeQuantity: document.getElementById("nodeQuantity").value
  }
}
const startMeasurement = async()=>{
  triggerWS = new WebSocket("ws://124.213.16.29:94/ws/connectTrigger");
  triggerWS.onopen = triggerOnOpen;
  triggerWS.onclose = triggerOnClose;
  triggerWS.onerror = triggerOnError;
  triggerWS.onmessage = triggerOnMessage;
  startMeasurementButton.remove();
  document.getElementById("projectStateButtons").append(stopMeasurementButton);
  shotPointButton.disabled = false;
}
const stopMeasurement = async()=>{
  var triggerWSPayload = {
    command: 'disableAsync'
  }
  triggerWS.send(JSON.stringify(triggerWSPayload));
  setTimeout(function() {}, 1000);
  triggerWS.close();
  shotPointButton.disabled = true;
  stopMeasurementButton.remove();
  document.getElementById("projectStateButtons").append(startMeasurementButton);
  document.getElementById("shotPointLabel").textContent = 0;
  document.getElementById("shotLabel").textContent = 0;
}

const newShotPoint  = async() => {
numShots=1;
numShotPoints++;
triggerFile[`shotPoint${numShotPoints}`]={};
var triggerWSPayload = {
    command: 'newShotPoint',
    params:{
      recordLength: document.getElementById("recordLength").value,
      sensitivity: document.getElementById("sensitivity").value,
      shotPoint: `${Number(document.getElementById("shotPointLabel").textContent)+1}`
    }
  }
  triggerWS.send(JSON.stringify(triggerWSPayload));
  document.getElementById("shotPointLabel").textContent = `${Number(document.getElementById("shotPointLabel").textContent)+1}`;
  document.getElementById("shotLabel").textContent = 0;
  shotPointButton.remove();
  document.getElementById("shotPointSwap").append(finishShotPointButton);
}

const finishShotPoint = async()=>{
  var triggerWSPayload = {
    command: 'finishShotPoint'
  }
  triggerWS.send(JSON.stringify(triggerWSPayload));
  finishShotPointButton.remove();
  document.getElementById("shotPointSwap").append(shotPointButton);
}

$('#sensitivity').ionRangeSlider({
  min     : 0,
  max     : 65536,
  type    : 'single',
  step    : 1024,
  postfix : '',
  prettify: false,
  hasGrid : true
})
saveFormButton.addEventListener('click', saveConfig);
startMeasurementButton.addEventListener('click',startMeasurement);
stopMeasurementButton.addEventListener('click',stopMeasurement);
shotPointButton.addEventListener('click',newShotPoint);
finishShotPointButton.addEventListener('click',finishShotPoint);
/************************************************
*
*                 HARVESTING PANE               *
*
*************************************************/
const getlistNodes = document.getElementById('listButton');
const collectDataButton = document.getElementById('collectDataButton');
const stopCollectingDataButton = document.createElement("button");
var stopCollectingDataButtonText = document.createElement("h4")
stopCollectingDataButton.setAttribute("class","btn btn-block btn-danger");
stopCollectingDataButton.setAttribute("id","stopDataButton");
stopCollectingDataButtonText.innerText = "Stop";
stopCollectingDataButton.appendChild(stopCollectingDataButtonText);
const downloadButton = document.getElementById("downloadData");
var nodeFile = {};

const downloadFile = () =>{
  var data = localStorage.getItem('payload');
  const blob = new Blob([data], {type : "text/plain"})
  const href = URL.createObjectURL(blob);
  const anchor = Object.assign(document.createElement("a"),{ href, style: "display:none", download: "Measurements.txt"});
  document.getElementById("availableList").appendChild(anchor);
  anchor.click();
  URL.revokeObjectURL(href);
  anchor.remove();
}
function delay(delayInms) {
  return new Promise(resolve => {
    setTimeout(() => {
      resolve(2);
    }, delayInms);
  });
}
const stopHarvesting = async() => {
  var ssid = $( "#availableList option:selected" ).text();
  try {
    var nodeWSPayload = {
      command: 'disableAsync'
    }
    nodeWS.send(JSON.stringify(nodeWSPayload));
    await delay(2000);
    var response = await sendHttpRequest('GET', 'http://124.213.16.29:80/api/v1/disconnectNode');
    if(response.status == 500){
      Toast.fire({
        icon: 'error',
        title: 'Desconexión ha fallado.'
      })
      return;
    }
    Toast.fire({
      icon: 'success',
      title: `${ssid} desconectado`
    })
    stopCollectingDataButton.remove();
    document.getElementById("changeButtons").append(collectDataButton);
  } catch (error) {
    Toast.fire({
      icon: 'error',
      title: 'Perdida de conexión'
    })
  }
};

const startHarvesting = async() => {
  var ssid = $( "#availableList option:selected" ).text();
  if (!(typeof (ssid) === 'string' && ['', 'null', 'undefined'].indexOf(ssid) == -1))
  {
    Toast.fire({
      icon: 'warning',
      title: 'Selecione un nodo para la conexión.'
    })
    return;
  }
  try {
    nodeCommand = "harvest";
    var response = await sendHttpRequest('POST', 'http://124.213.16.29:80/api/v1/connectNode', {ssid:ssid.trim()});
    nodeWS = new WebSocket("ws://124.213.16.29:94/ws/connectNode");
    nodeWS.onopen = nodeOnOpen;
    nodeWS.onclose = nodeOnClose;
    nodeWS.onerror = nodeOnError;
    nodeWS.onmessage = nodeOnMessage;
    if(response.status == 500){
      // ws.close()
      Toast.fire({
        icon: 'error',
        title: 'Conexión ha fallado.'
      })
      return;
    }
    Toast.fire({
      icon: 'success',
      title: `${ssid} conectado`
    })
  
    collectDataButton.remove();
    document.getElementById("changeButtons").append(stopCollectingDataButton);
    } catch (error) {
      Toast.fire({
        icon: 'error',
        title: 'Perdida de conexión'
      })
    }
};


const listNodes = async() => {
  try {
    var response = await sendHttpRequest('GET', 'http://124.213.16.29:80/api/v1/scanNodes');
    const {nodes} = await response.json();
    if(response.status == 500){
      Toast.fire({
        icon: 'error',
        title: 'Scan Failed'
      })
      return;
    }
    $('.modal-backdrop').hide();
    $('.modal').hide();
    var nodeList = document.getElementById("availableList").length
    // console.log(nodeList);
    if(nodeList != 0){
      for (let index = 0; index < nodeList; index++) {
        document.getElementById("availableList").options.remove(0);
      }
    }
    else{
      $("#cardNodesList").removeClass("collapsed-card");
    }
    nodes.forEach(element => {
      $("#availableList").append(`<option> ${element} </option>`);

    });
    for (let index = 0; index < document.getElementById("availableList").length ; index++) {
      document.getElementById("availableList").children.item(index).onclick = ()=>{
        console.log(document.getElementById("availableList").children.item(index).innerText);
        document.getElementById("selectedNodeLabel").innerHTML = "Selected: \n"+document.getElementById("availableList").children.item(index).innerText;
      }
      document.getElementById("availableList").children.item(index).onselect = ()=>{
        console.log(document.getElementById("availableList").children.item(index).innerText);
        document.getElementById("selectedNodeLabel").innerHTML = "Selected: \n"+document.getElementById("availableList").children.item(index).innerText;
      }
    }
    Toast.fire({
      icon: 'success',
      title: 'Scan realizado.'
    })
  } catch (error) {
    Toast.fire({
      icon: 'error',
      title: 'Perdida de conexión'
    })
  }
};

getlistNodes.addEventListener('click', listNodes);
collectDataButton.addEventListener('click', startHarvesting);
stopCollectingDataButton.addEventListener('click',stopHarvesting);
downloadButton.addEventListener('click',downloadFile);

/************************************************
*                                               *
*           WEBSOCKET EVENTS TRIGGER            *
*                                               *
*************************************************/
var triggerOnOpen = function() {
  console.log('CONNECTED');
  var triggerWSPayload = {
    command: 'enableAsync'
  }
  triggerWS.send(JSON.stringify(triggerWSPayload));
  numShots=1;
};

var triggerOnClose = function() {
  console.log('CLOSED');
  triggerWS = null;
  console.log(triggerFile);
  console.log(JSON.stringify(triggerFile));
  localStorage.setItem(`${document.getElementById("Name").value}-Quick-Trigger`, JSON.stringify(triggerFile));
  numShots=0;
  numShotPoints=0;
};

var triggerOnError = function(error){
  console.log(error);
}

var triggerOnMessage = function(event){
  console.log(event.data);
  var shotRecieved = event.data.split(";")[0].split(",");
  triggerFile[`shotPoint${numShotPoints}`][`shot${numShots}`] ={
    time: shotRecieved[0],
    adcSample: shotRecieved[1],
    latitude: shotRecieved[2],
    longitude: shotRecieved[3],
    tiemstamp: shotRecieved[4],
    secCounter: shotRecieved[5],
    shootPoint: shotRecieved[6]
  };
  numShots++;
  document.getElementById("shotLabel").textContent = `${Number(document.getElementById("shotLabel").textContent)+1}`;
}
/************************************************
*                                               *
*           WEBSOCKET EVENTS NODE               *
*                                               *
*************************************************/
var measurementData;
var measurementShotPoints;
var nodeOnOpen = function() {
  if(["harvest"].indexOf(nodeCommand) != -1){
    // var ssid = $( "#availableList option:selected" ).text();
    console.log('CONNECTED TO NODE');
    // payload = `${ssid.trim()}/`;
    var nodeWSPayload = {
      command: 'enableAsync'
    }
    nodeWS.send(JSON.stringify(nodeWSPayload));
    measurementData = JSON.parse(localStorage.getItem(`${document.getElementById("Name").value}-Quick-Trigger`));
    measurementShotPoints = Object.keys(measurementData);
  }
  else{
    var nodeWSPayload = {
      command: 'testNode'
    }
    nodeWS.send(JSON.stringify(nodeWSPayload));
  }
  
};

var nodeOnClose = function() {
  console.log('CLOSED');
  nodeWS = null;
  localStorage.setItem(`${document.getElementById("Name").value}-${$( "#availableList option:selected" ).text()}`, JSON.stringify(nodeFile));
};

var nodeOnError = function(error){
  console.log(error);
}

var currentShotPoint = 1;
var currentShot = 0;
var nodeOnMessage = function(event){
  console.log(event.data);
  console.log(JSON.parse(event.data).command);
  if (["enableAsync"].indexOf(JSON.parse(event.data).command) != -1) {

    if(["harvest"].indexOf(nodeCommand) != -1){
      if((typeof(measurementData) == 'object') && currentShotPoint < measurementShotPoints.length){
        var shotpoint = measurementData[Object.keys(measurementData)[currentShotPoint]]
        console.log(shotpoint);
        if (currentShot < Object.keys(shotpoint).length) {
          var {time,secCounter} = shotpoint[Object.keys(shotpoint)[currentShot]];
          console.log(time,secCounter)
          var nodeWSPayload = {
            command: 'harvestShotPoint',
            params:{
              time,
              secCounter,
              recordLength: measurementData.config.recordLength
            }
          }
          nodeWS.send(JSON.stringify(nodeWSPayload));
        }
      }
    }
    else{
      var nodeWSPayload = {
        command: 'testMode'
      }
      nodeWS.send(JSON.stringify(nodeWSPayload));
    }
  }
  else if(["disableAsync"].indexOf(JSON.parse(event.data).command) != -1){
    nodeWS.close();
  }
  if(["harvestNode"].indexOf(JSON.parse(event.data).command) != -1){
    if((typeof(measurementData) == 'object') && currentShotPoint < measurementShotPoints.length){
      var shotpoint = measurementData[Object.keys(measurementData)[currentShotPoint]]
      console.log(shotpoint);
      if (currentShot < Object.keys(shotpoint).length) {
        var {time,secCounter} = shotpoint[Object.keys(shotpoint)[currentShot]];
        console.log(time,secCounter)
        var nodeWSPayload = {
          command: 'harvestShotPoint',
          params:{
            time,
            secCounter,
            recordLength: measurementData.config.recordLength
          }
        }
        nodeWS.send(JSON.stringify(nodeWSPayload));
      }
    }
  }
  // console.log(event.data.split("/"));
  // payload = payload + event.data
}

