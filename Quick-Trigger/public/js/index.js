var Toast = Swal.mixin({
  toast: true,
  showConfirmButton: false,
  timer: 2000
});
var payload;
var harvestWS;
var testWS;
var triggerWS;

function delay(delayInms) {
  return new Promise(resolve => {
    setTimeout(() => {
      resolve(2);
    }, delayInms);
  });
}
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


var chartT = new Highcharts.Chart({
  chart:{ renderTo : 'chart-testing' },
  title: { text: 'Real-Time' },
  series: [{
    showInLegend: false,
    data: [15000, 32000, 60000]
  }],
  plotOptions: {
    line: { animation: false,
      dataLabels: { enabled: false }
    },
    series: { color: '#059e8a' }
  },
  tooltip: {
    animation: false,
      dataLabels: { enabled: false },
      enabled:false,
    },
  xAxis: { text: 'muestra'
  },
  yAxis: {
    title: { text: 'Valor'},
    min: 0,
    max: 65600
  },
  credits: { enabled: false }
})


// var data = [];
// var dataset;
// var totalPoints = 1251;
// var updateInterval = 10;
// var now = new Date().getTime();

// var i=0;
// function initData() {
//     data.shift();
//     while (data.length < totalPoints) {
//       y=0;
//       data.push(y)
//     }
//     // Zip the generated y values with the x values
//     var res = []
//     for (var i = 0; i < data.length; ++i) {
//       res.push([i, data[i]])
//     }
//     return res
// }

// function GetData() {
//     data.shift();
//     while (data.length < totalPoints) {
//         if(i>=totalPoints){
//           i=0;
//         }
//         data.push([ now += updateInterval, data[i]]);
//         ++i;
//     }
// }

// var options = {
//     grid: {
//         borderColor: '#f3f3f3',
//         borderWidth: 1,
//         tickColor: '#f3f3f3'
//     },
//     series: {
//         color: '#3c8dbc',
//         lines: {
//         lineWidth: 2,
//         show: true,
//         fill: false,
//         },
//     },
//     yaxis: {
//         min: 0,
//         max: 1,
//         show: true
//     },
//     xaxis:{
//         mode: "time",
//         tickSize: [200, "second"],
//         axisLabel: "Time",
//         axisLabelUseCanvas: true,
//         axisLabelFontSizePixels: 12,
//         axisLabelFontFamily: 'Verdana, Arial',
//         axisLabelPadding: 10
//     }
// }

// initData();

// dataset = [{data}];

// $.plot($("#interactive"), dataset, options);

// function update() {
//   console.log("update");
//     console.log(data);
//     GetData();
//     $.plot($("#interactive"), dataset, options)
//     // setTimeout(update, updateInterval);
// }
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
    var testWSPayload = {
      command: 'disableAsync'
    }
    testWS.send(JSON.stringify(testWSPayload));
    testWS.close();
    await delay(1000);
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
    testWS = new WebSocket("ws://124.213.16.29:94/ws/connectNode");
    testWS.onopen = testOnOpen;
    testWS.onclose = testOnClose;
    testWS.onerror = testOnError;
    testWS.onmessage = testOnMessage;
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
      document.getElementById("deviceList").onchange = ()=>{
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
var matrices=[];
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
    Toast.fire({
      icon: 'warning',
      title: 'Complete la configuracion del proyecto'
    })
    return;
  }
  document.getElementById("nameLabel").textContent = document.getElementById("Name").value;
  document.getElementById("recordLengthLabel").textContent = document.getElementById("recordLength").value;
  document.getElementById("nodeQuantityLabel").textContent = document.getElementById("nodeQuantity").value;
  document.getElementById("shotPointLabel").textContent = 0;
  document.getElementById("shotLabel").textContent = 0;
  let date = new Date();
  let day = date.getDate()
  let month = date.getMonth() + 1;
  let year = date.getFullYear();
  triggerFile["trigger"]={}
  triggerFile["trigger"]["config"]={
    recordLength: document.getElementById("recordLength").value,
    nodeQuantity: document.getElementById("nodeQuantity").value,
    date: (month < 10) ? `${day}-0${month}-${year}` : `${day}-${month}-${year}` 
  }
}
const startMeasurement = async()=>{
  // if (localStorage.getItem(projectName.value) == null)
  // {
  //   Toast.fire({
  //     icon: 'warning',
  //     title: 'Guarde la configuracion del proyecto'
  //   })
  //   return;
  // }
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
triggerFile["trigger"][`shotPoint${numShotPoints}`]={};
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
  stopMeasurementButton.disabled = true;
}

const finishShotPoint = async()=>{
  var triggerWSPayload = {
    command: 'finishShotPoint'
  }
  triggerWS.send(JSON.stringify(triggerWSPayload));
  finishShotPointButton.remove();
  document.getElementById("shotPointSwap").append(shotPointButton);
  stopMeasurementButton.disabled = false;
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
const listShootpoints = document.getElementById("id_shootpoint");
const listShoots = document.getElementById("id_shoot");
var stopCollectingDataButtonText = document.createElement("h4")
stopCollectingDataButton.setAttribute("class","btn btn-block btn-danger");
stopCollectingDataButton.setAttribute("id","stopDataButton");
stopCollectingDataButtonText.innerText = "Stop";
stopCollectingDataButton.appendChild(stopCollectingDataButtonText);
const downloadButton = document.getElementById("downloadData");
//_______________________________________________________________________________________________
listShootpoints.addEventListener("change",function(e){
  var optionsNumber=listShoots.length;
  var id_sp=parseInt(listShootpoints.value);
  var shoots_in_id_sp=matrices[id_sp-1].arraymatriz.length;
  if(optionsNumber != 0){
    for (let index = 0; index < optionsNumber; index++) {
      listShoots.options.remove(0);
    }
  }
  for (let index = 0; index < shoots_in_id_sp; index++) {
    var option = document.createElement("option");
    listShoots.append(option);
    listShoots.children[index].text = `${(index+1)}`;
  }
  listShoots.value=listShoots[0].value;

  Graficar();

});

listShoots.addEventListener("change",function(e){
  Graficar();
})
//__________________________________________________________________________________________________
if(localStorage.length > 0){
  for (let index = 0; index < localStorage.length; index++) {
    var file = JSON.parse(localStorage.getItem(localStorage.key(index)));
    var title = localStorage.key(index)
    if(Object.keys(file).length > 0){
      var numberShotpoints = Object.keys(file.trigger).length - 1;
      var date = file.trigger.config.date;
      var tdTitle = document.createElement("td");
      tdTitle.textContent = title;
      var tdShotpoints = document.createElement("td");
      tdShotpoints.textContent = `${numberShotpoints}`;
      var tdDate = document.createElement("td");
      tdDate.textContent = date;
      var trFile = document.createElement("tr");
      trFile.setAttribute("id",`file${index}`)
      trFile.append(tdTitle);
      trFile.append(tdShotpoints);
      trFile.append(tdDate);
      var table = document.getElementById("measurementsTable");
      table.append(trFile);
    }
  }
}

// var nodeFile = {};

const downloadFile = () =>{

  // var data = "";
  // for (let ishp = 0; ishp < matrices.length; ishp++) {
  //   data = data.concat(`shotpoint ${ishp}`)
  //   for (let index = 0; index < array.length; index++) {
  //     const element = array[index];
      
  //   }
    
  // }





  var data = localStorage.getItem('payload');
  const blob = new Blob([data], {type : "text/plain"})
  const href = URL.createObjectURL(blob);
  const anchor = Object.assign(document.createElement("a"),{ href, style: "display:none", download: "Measurements.txt"});
  document.getElementById("availableList").appendChild(anchor);
  anchor.click();
  URL.revokeObjectURL(href);
  anchor.remove();
}
const stopHarvesting = async() => {
  var ssid = $( "#availableList option:selected" ).text();
  try {
    var harvestWSPayload = {
      command: 'disableAsync'
    }
    harvestWS.send(JSON.stringify(harvestWSPayload));
    await delay(1000);
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
    var regex =/(\d+)/g;
    id_nodo_cosecha=Number.parseInt($( "#availableList option:selected" ).text().trim().match(regex))-1;
    var response = await sendHttpRequest('POST', 'http://124.213.16.29:80/api/v1/connectNode', {ssid:ssid.trim()});
    harvestWS = new WebSocket("ws://124.213.16.29:94/ws/connectNode");
    harvestWS.onopen = harvestOnOpen;
    harvestWS.onclose = harvestOnClose;
    harvestWS.onerror = harvestOnError;
    harvestWS.onmessage = harvestOnMessage;
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
      document.getElementById("availableList").onchange = ()=>{
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
  localStorage.setItem(`${document.getElementById("Name").value}`, JSON.stringify(triggerFile));
  numShots=0;
  numShotPoints=0;

  //______________________________________________________________________________________________________________________
  //creacion de matrices para las graficas

  //contar shoots
  
  measurementData = JSON.parse(localStorage.getItem(`${document.getElementById("Name").value}`));
  measurementData = measurementData.trigger;
var auxrecordLength= Number.parseFloat(measurementData.config['recordLength']);
var auxno_canales=Number.parseFloat(measurementData.config['nodeQuantity']);
var No_shootpoints=Object.keys(measurementData).length-1;

 for(let i=0; i<No_shootpoints;i++){ 
  var No_shoots=Object.keys(measurementData[Object.keys(measurementData).splice(1)[i]]).length;
  for(let j=0;j<No_shoots;j++){
    matrices[i]=new Arraymatriz(No_shoots,auxno_canales,auxrecordLength);
  }
}

console.log(matrices);
var optionsNumber = listShootpoints.length;
if(optionsNumber != 0){
  for (let index = 0; index < optionsNumber; index++) {
    listShootpoints.options.remove(0);
  }
}
for (let index = 0; index < No_shootpoints; index++) {
  var option = document.createElement("option");
  listShootpoints.append(option);
  listShootpoints.children[index].text = `${(index+1)}`;
}
listShootpoints.value=listShootpoints[0].value;

var id_sp=parseInt(listShootpoints.value);
  var shoots_in_id_sp=matrices[id_sp-1].arraymatriz.length;
  if(optionsNumber != 0){
    for (let index = 0; index < optionsNumber; index++) {
      listShoots.options.remove(0);
    }
  }
  for (let index = 0; index < shoots_in_id_sp; index++) {
    var option = document.createElement("option");
    listShoots.append(option);
    listShoots.children[index].text = `${(index+1)}`;
  }
listShoots.value=listShoots[0].value;
  Graficar();
//____________________________________________________________________________________________________________________________
};

var triggerOnError = function(error){
  console.log(error);
}

var triggerOnMessage = function(event){
  console.log(event.data);
  var shotRecieved = event.data.split(";")[0].split(",");
  triggerFile["trigger"][`shotPoint${numShotPoints}`][`shot${numShots}`] ={
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
*       WEBSOCKET EVENTS NODE - HARVEST         *
*                                               *
*************************************************/

//__________________________________________________________________________________________________________________________________

//datos measurementData
//object.keys(measu)
//splice
// JSON.parse(localStorage.getItem(localStorage.key(13)))
class MatrizGrafica{
  constructor(no_canales, duracion){
    this.no_canales=no_canales;
    this.duracion=Number.parseInt(500*duracion);
    this.matriz=[];
    for(let i=0;i<no_canales;i++){
      this.matriz[i]=new Float32Array(this.duracion);
    }
  }
  
  matriz_init (datos_canal,id_canal) {
    
    
    for(let i=0; i<this.matriz[id_canal].length; i++){
       this.matriz[id_canal][i]=datos_canal[i];
    }
  }
  
 get_matriz(){
     return this.matriz;
 }
   
}

class Arraymatriz{
  constructor(longitud,no_canales,duracion){
  this.arraymatriz=[];
  for(let i=0; i<longitud;i++){
      this.arraymatriz[i]=new MatrizGrafica(no_canales,duracion);}
  }
  }

var Graficar=function(){
  console.log("shp"+listShootpoints.value+"sh"+listShoots.value);

  var trazas=[]; //data de la grafica, vector de n nodos
  var index_shp=Number.parseInt(listShootpoints.value)-1; //seleccion en datalist de shootpoint
  var index_sh=Number.parseInt(listShoots.value)-1; //Seleccion en datalist de shoot
  var no_canales=matrices[0].arraymatriz[0].no_canales; //cantidad de canales de la grafica

  
  for(let i=0;i<no_canales;i++){
    var valormaximo=Math.max.apply(null, (matrices[index_shp].arraymatriz[index_sh].matriz[i].map(Math.abs)));
    var traza_norm=[];
    for(let j=0;j<matrices[index_shp].arraymatriz[index_sh].matriz[i].length;j++){
      traza_norm[j]=(matrices[index_shp].arraymatriz[index_sh].matriz[i][j]/(2*valormaximo))+i;
    }
    trazas[i]={data: traza_norm}
  }




 var chartH=new Highcharts.chart({
    chart: {
        renderTo : 'chart-harv',
        type: 'spline',
        inverted: true
    },
    title: {
        text: 'Canales'
    },
    xAxis: {
        reversed: false,
        title: {
            enabled: true,
            text: 'Tiempo'
        },
        labels: {
            format: '{value} ms'
        },
        accessibility: {
          rangeDescription: 'Range: 0 to '+`${matrices[0].arraymatriz[0].duracion*2}`
      },
        maxPadding: 0.05,
        showLastLabel: false
    },
    yAxis: {
        title: {
            text: 'Canal'
        },
        labels: {
            format: '{value}'
        },
        lineWidth: 2
    },
    legend: {
        enabled: false
    },
    tooltip: {
        enabled: false
    },
    plotOptions: {
        pointStart: 0,
        spline: {
            marker: {
                enable: false
            }
        }
    },
    series: trazas
});
}


//__________________________________________________________________________________________________________________________________
var currentShotPoint = 1;
var currentShot = 0;
var nodeData = [];
var testingData = [];
var measurementData;
var measurementShotPoints;
var harvestOnOpen = function() {
  // var ssid = $( "#availableList option:selected" ).text();
  console.log('CONNECTED TO NODE');
  // payload = `${ssid.trim()}/`;
  var harvestWSPayload = {
    command: 'enableAsync'
  }
  harvestWS.send(JSON.stringify(harvestWSPayload));
  measurementData = JSON.parse(localStorage.getItem(`${document.getElementById("Name").value}`));
  measurementData = measurementData["trigger"];
  measurementShotPoints = Object.keys(measurementData);
};

var harvestOnClose = function() {
  console.log('CLOSED');
  harvestWS = null;
  // localStorage.setItem(`${document.getElementById("Name").value}-${$( "#availableList option:selected" ).text().trim()}`, JSON.stringify(nodeFile));

//-------------------Colocar aca relleno de las matrices
//localStorage.getItem(localStorage.key(13)).replace(/\\/g, '') para quitar backslash de los datos
//JSON.parse(localStorage.getItem(localStorage.key(10)))

};

var harvestOnError = function(error){
  console.log(error);
}

var shotpoint,shot,time,secCounter;
var filtro='';

var harvestOnMessage = async function(event){
  try {
    if (["enableAsync"].indexOf(JSON.parse(event.data).command) != -1) {
      currentShotPoint = 1;
      currentShot = 0;
      // nodeFile[`shotPoint${currentShotPoint}`]={};
      shotpoint = measurementData[Object.keys(measurementData)[currentShotPoint]];
      console.log(shotpoint[Object.keys(shotpoint)[currentShot]]);
      var {time,secCounter} = shotpoint[Object.keys(shotpoint)[currentShot]];
      // nodeFile[`shotPoint${currentShotPoint}`]={};
      console.log(`shotPoint: ${currentShotPoint}  shot: ${currentShot}`);
      if((typeof(measurementData) == 'object') && currentShotPoint < measurementShotPoints.length){
        if (currentShot < Object.keys(shotpoint).length) {
          console.log(time,secCounter)
          var harvestWSPayload = {
            command: 'harvestShotPoint',
            params:{
              time,
              secCounter,
              recordLength: measurementData.config.recordLength
            }
          }
          harvestWS.send(JSON.stringify(harvestWSPayload));
        }
      }
    }
    else if(["disableAsync"].indexOf(JSON.parse(event.data).command) != -1){
      harvestWS.close();
    }
    // else if(["idle"].indexOf(JSON.parse(event.data).command) != -1){
    //   if(nodeData.length > 0)
    //   {
    //     nodeFile[`shotPoint${currentShotPoint}`][`shot${currentShot}`] ={
    //       data: nodeData
    //     };
    //     console.log("hola");
    //     console.log(nodeData);
        
    //     definir id_nodo basado a cual nodo se cosechó
    //     ____________________________________________________________________________________________________
    //     var id_nodo=0;
    //     var aux=new Float32Array(matrices[0].arraymatriz[0].duracion)
    //     for(let i=0;i<aux.length;i++){
    //       aux[i]=Math.sin(2*Math.PI*i/25);
    //     }

    //     matrices[currentShotPoint-1].arraymatriz[currentShot-1].matriz_init(aux,id_nodo);
    //     _____________________________________________________________________________________________________
        
    //     nodeData = [];
        
    //   }
    //   shotpoint = measurementData[Object.keys(measurementData)[currentShotPoint]];
    //   if (currentShot >= Object.keys(shotpoint).length) {
    //     currentShot = 0;
    //     currentShotPoint++;
    //     console.log("cambio de shotpoint");
    //     if(currentShotPoint >=  measurementShotPoints.length){
    //       console.log("end transmission")
    //       stopCollectingDataButton.click();
    //       return;
    //     }
    //     nodeFile[`shotPoint${currentShotPoint}`]={};
    //   }
    //   shot = shotpoint[Object.keys(shotpoint)[currentShot]];
    //   console.log("shot",shot);
    //   time = shotpoint[Object.keys(shotpoint)[currentShot]].time;
    //   secCounter = shotpoint[Object.keys(shotpoint)[currentShot]].secCounter
    //   console.log(`shotPoint: ${currentShotPoint}  shot: ${currentShot}`);
    //   console.log(time,secCounter)
    //   console.log("hola2");
    //   var harvestWSPayload = {
    //     command: 'harvestShotPoint',
    //     params:{
    //       time,
    //       secCounter,
    //       recordLength: measurementData.config.recordLength
    //     }
    //   }
    //   console.log("hola3");
    //   harvestWS.send(JSON.stringify(harvestWSPayload));
    //   currentShot++;
    // }  
  } catch (error) {
    if(!event.data.includes("command"))
    { 
      if(event.data.includes("k")){
        console.log("inicio trama")
        filtro=event.data.replace(/k/g, '');
      }
      console.log(filtro)
      if(filtro.includes("f")){
        console.log("fin trama")
        filtro=filtro.replace(/f/g, '');
        console.log(filtro);
        nodeData = nodeData.concat(filtro.split(";"));
        var floatData = [];
        for (let index = 0; index < nodeData.length; index++) {
          const element = nodeData[index];
          var dato = Number.parseFloat(element)
          if(dato == NaN){
            try {
              floatData[index]=floatData[index-1];
            } catch (error) {
              floatData[index]=0.0;
            }
          }
          else{
            floatData[index] = dato;
          }
        }
        console.log(floatData);
        console.log(nodeData);
        matrices[currentShotPoint-1].arraymatriz[currentShot].matriz_init(floatData,id_nodo_cosecha);
        nodeData = [];

        currentShot++;
        if (currentShot >= Object.keys(shotpoint).length) {
          currentShot = 0;
          currentShotPoint++;
          console.log("cambio de shotpoint");
          shotpoint = measurementData[Object.keys(measurementData)[currentShotPoint]];
          if(currentShotPoint >=  measurementShotPoints.length){
            console.log("end transmission")
            stopCollectingDataButton.click();
            return;
          }
        }
        shot = shotpoint[Object.keys(shotpoint)[currentShot]];
        console.log("shot",shot);
        time = shotpoint[Object.keys(shotpoint)[currentShot]].time;
        secCounter = shotpoint[Object.keys(shotpoint)[currentShot]].secCounter
        console.log(`shotPoint: ${currentShotPoint}  shot: ${currentShot}`);
        console.log(time,secCounter)
        console.log("hola2");
        var harvestWSPayload = {
          command: 'harvestShotPoint',
          params:{
            time,
            secCounter,
            recordLength: measurementData.config.recordLength
          }
        }
        console.log("hola3");
        harvestWS.send(JSON.stringify(harvestWSPayload));
      }
      else{
        filtro = filtro.concat(event.data);
      }

      //console.log(filtro);
     /* filtro=event.data.replace(/k/g, '');
      filtro=filtro.replace(/\\/g,'');
      filtro=filtro.replace(/n/g,'');
      filtro=filtro.replace(/r/g,'');
      */
     //nodeData = nodeData.concat(event.data.split("\r\n"));
      
    }
  } 
}

/************************************************
*                                               *
*       WEBSOCKET EVENTS NODE - TESTING         *
*                                               *
*************************************************/
var testOnOpen = function() {
  console.log('CONNECTED TO NODE');
  // payload = `${ssid.trim()}/`;
  var testWSPayload = {
    command: 'enableAsync'
  }
  testWS.send(JSON.stringify(testWSPayload));  
};

var testOnClose = function() {
  console.log('CLOSED');
  testWS = null;
};

var testOnError = function(error){
  console.log(error);
}

var testOnMessage = function(event){
  try {
    if (["enableAsync"].indexOf(JSON.parse(event.data).command) != -1) {
      var testWSPayload = {
        command: 'testMode'
      }
      testWS.send(JSON.stringify(testWSPayload)); 
    }
    else if(["disableAsync"].indexOf(JSON.parse(event.data).command) != -1){

    }else if(["idle"].indexOf(JSON.parse(event.data).command) != -1){
      console.log("hola");
    }  
  } catch (error) {
    
    if(!event.data.includes("command"))
    {
      var buff_aux=event.data.split("\r\n");
      
      testingData = testingData.concat(buff_aux.slice(1,buff_aux.length-1));
      if(testingData.length>230){
        //console.log(testingData);
        var y=[];
        for(let i=0;i<testingData.length;i++){
            y[i]=Number.parseFloat(testingData[i]);
        }
        //console.log(Number.parseFloat(testingData[0]));
      chartT.series[0].setData(y);
      testingData=[];
      //console.log([4,3,6]);
      
      }
      
    }
  } 
}