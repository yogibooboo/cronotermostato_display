if (window.location.hostname!='') var gateway = `ws://${window.location.hostname}/ws`; 
else var gateway = `ws://192.168.1.103/ws`;
var websocket;


window.addEventListener('load', onload); 

/* per l'aggiustamento schermo vedremo

var adjustscreen=function(){
	zm=1,wh=$(window).height(),ww=$(window).width();
	//if ((wh>=750)&&(ww>=1024)) zm=Math.min(wh/750,ww/1024)
	//else{
		//if (wh<750) 
		zm=wh/750;
		//if (ww<1024) 
		zm=Math.min(zm,ww/1024);
		zm*=0.98;
	//} 
 	$("body").css({transform:"scale("+zm+")"});
 	$("#campogioco").css({"position":"absolute","left":(ww/zm-1024)/2})
 	
};
adjustscreen();

$(window).resize(function () {
	scala.offsetxx=$("#campogioco").offset().left;
	scala.offsetyy=$("#campogioco").offset().top;
	adjustscreen();
});

*/

function onload(event) {
initWebSocket();
//getCurrentValue();
}






canvascrono = document.getElementById("canvascrono");
ctxmt = canvascrono.getContext('2d');
canvascrono.width=    $("#cursoridiv").width();
canvascrono.height=    $("#cursoridiv").height();

tmcrono=new Image();
tmcrono.src="cronotermostato_fondo.jpg";

tmcursore=new Image();
tmcursore.src="cursore.png";
tmcursoresel=new Image();
tmcursoresel.src="cursoresel.png";
tmcursoreblu=new Image();
tmcursoreblu.src="cursoreblu.png";



function gestiscionoff(commuta){ 
	if (commuta) programma[200]=programma[200]^1;
	if (programma[200]==1){
		// on
	
		document.getElementById("onoff").style.backgroundColor='green';
		document.getElementById("scrittaonoff").textContent="TERMOSTATO ON"
	}
	else {
		
		document.getElementById("onoff").style.backgroundColor='#aaaaaa';
		document.getElementById("scrittaonoff").textContent="TERMOSTATO OFF"
	}
	if (commuta) inviasettaggi();

}

function gestisciautoman(){
	document.getElementById("scrittamanuale").textContent="MANUALE   "+ (programma[202]/10).toFixed(1)+" "+String.fromCharCode(0x00B0)+"C";
	if (programma[201]!=0){
		//manuale
		document.getElementById("manuale").style.backgroundColor='green';
		document.getElementById("automatico").style.backgroundColor='#aaaaaa';
	} else {
		//automatico
		document.getElementById("automatico").style.backgroundColor='green';
		document.getElementById("manuale").style.backgroundColor='#aaaaaa';
	}
	tcrono.initcrono();
}

function aggiornaselezione(selezione){
	for(i=0;i<7;i++){
		if (i==selezione){
			indicedatadisplay=selezione;
			document.getElementById("day"+i).style.backgroundColor='green';
			
		}
		else {
			document.getElementById("day"+i).style.backgroundColor='#aaaaaa';
			flagselezionegiorno=true;
		
		}
	}
	tcrono.initcrono();	
}


//pulsantiera
document.getElementById("invia").addEventListener("click", function()  {inviasettaggiesoglie()} );
document.getElementById("annulla").addEventListener("click", function()  {annullasettaggi()} ); 

document.getElementById("day0").addEventListener("click", function()  {aggiornaselezione(0)} );
document.getElementById("day1").addEventListener("click", function()  {aggiornaselezione(1)} );
document.getElementById("day2").addEventListener("click", function()  {aggiornaselezione(2)} );
document.getElementById("day3").addEventListener("click", function()  {aggiornaselezione(3)} );
document.getElementById("day4").addEventListener("click", function()  {aggiornaselezione(4)} );
document.getElementById("day5").addEventListener("click", function()  {aggiornaselezione(5)} );
document.getElementById("day6").addEventListener("click", function()  {aggiornaselezione(6)} );



document.getElementById("onoff").addEventListener("click", function()  {gestiscionoff(true)} );

document.getElementById("tipocursori").addEventListener("click", function()  {tcrono.initcrono()} );	 
document.getElementById("tipografico").addEventListener("click", function()  {tcrono.initcrono()} );	
document.getElementById("vediumidita").addEventListener("click", function()  {gestiscivediumidita()} );
document.getElementById("veditemperatura").addEventListener("click", function()  {gestisciveditemperatura()} );
document.getElementById("vedibarra").addEventListener("click", function()  {gestiscivedibarra()} );

document.getElementById("manuale").addEventListener("click", function()  {programma[201]=1;gestisciautoman();inviasettaggi()} );
document.getElementById("manualepiu").addEventListener("click", function()  {programma[202]+=5;gestisciautoman();inviasettaggi()} );
document.getElementById("manualemeno").addEventListener("click", function()  {programma[202]-=5;gestisciautoman();inviasettaggi()} );
document.getElementById("automatico").addEventListener("click", function()  {programma[201]=0;gestisciautoman();inviasettaggi()} );

//bottoni nel canvas grafico
document.getElementById("programma0").addEventListener("click", function()  {visualizzaprogramma(0)} );
document.getElementById("programma1").addEventListener("click", function()  {visualizzaprogramma(1)} );
document.getElementById("programma2").addEventListener("click", function()  {visualizzaprogramma(2)} );
document.getElementById("programma3").addEventListener("click", function()  {visualizzaprogramma(3)} );
document.getElementById("comandoimpostazioni").addEventListener("click", function()  {comandoimpostazioni()} );
document.getElementById("inunora").addEventListener("click", function()  {inunora=true;tcrono.initcrono()} );
document.getElementById("inmezzora").addEventListener("click", function()  {inunora=false;tcrono.initcrono()} );

//form selezione banco da aggiornare
document.getElementById("selprogramma0").addEventListener("click", function()  {aggiornapulsantiselezione(0)} );
document.getElementById("selprogramma1").addEventListener("click", function()  {aggiornapulsantiselezione(1)} );
document.getElementById("selprogramma2").addEventListener("click", function()  {aggiornapulsantiselezione(2)} );
document.getElementById("selprogramma3").addEventListener("click", function()  {aggiornapulsantiselezione(3)} );

document.getElementById("selprogrammaannulla").addEventListener("click", function()  {annullasettaggi()} ); 
document.getElementById("selprogrammasalva").addEventListener("click", function()  {salvasettaggi()} ); 
document.getElementById("selprogrammacambianome").addEventListener("click", function()  {selprogrammacambianome()} ); 

// form impostazioni



document.getElementById("programmafisso").addEventListener("click", function()  {programma[221]=0} );
document.getElementById("programmasettimanale").addEventListener("click", function()  {programma[221]=1} );
document.getElementById("eccezione").addEventListener("click", function()  {programma[215]^=1;document.getElementById("eccezione").checked=programma[215]} );

document.getElementById("selprogrammafisso").addEventListener("click", function()  {selezionaprogramma(222,this)} );
document.getElementById("selprogrammasett0").addEventListener("click", function()  {selezionaprogramma(223,this)} );
document.getElementById("selprogrammasett1").addEventListener("click", function()  {selezionaprogramma(224,this)} );
document.getElementById("selprogrammasett2").addEventListener("click", function()  {selezionaprogramma(225,this)} );
document.getElementById("selprogrammasett3").addEventListener("click", function()  {selezionaprogramma(226,this)} );
document.getElementById("selprogrammasett4").addEventListener("click", function()  {selezionaprogramma(227,this)} );
document.getElementById("selprogrammasett5").addEventListener("click", function()  {selezionaprogramma(228,this)} );
document.getElementById("selprogrammasett6").addEventListener("click", function()  {selezionaprogramma(229,this)} );
document.getElementById("selprogrammaeccezione").addEventListener("click", function()  {selezionaprogramma(220,this)} );

document.getElementById("datainizioeccezione").addEventListener("change", function()  {cambiadatainizio()} );
document.getElementById("datafineeccezione").addEventListener("change", function()  {cambiadatafine()} );

document.getElementById("correzionepiu").addEventListener("click", function()  {correzionepiu();refreshcorrezioneisteresi()} );
document.getElementById("correzionemeno").addEventListener("click", function()  {correzionemeno();refreshcorrezioneisteresi()} );
document.getElementById("isteresipiu").addEventListener("click", function()  {isteresipiu();refreshcorrezioneisteresi()} );
document.getElementById("isteresimeno").addEventListener("click", function()  {isteresimeno();refreshcorrezioneisteresi()} );

document.getElementById("impostazionisalva").addEventListener("click", function()  {impostazionisalva()} );
document.getElementById("impostazioniannulla").addEventListener("click", function()  {impostazioniannulla()} );

//form generico selezione banco
document.getElementById("nprogrammasel0").addEventListener("click", function()  {selezionebottonesel(0)} );
document.getElementById("nprogrammasel1").addEventListener("click", function()  {selezionebottonesel(1)} );
document.getElementById("nprogrammasel2").addEventListener("click", function()  {selezionebottonesel(2)} );
document.getElementById("nprogrammasel3").addEventListener("click", function()  {selezionebottonesel(3)} );

document.getElementById("nprogrammaselannulla").addEventListener("click", function()  {nprogrammaselsalva(false)} );
document.getElementById("nprogrammaselsalva").addEventListener("click", function()  {nprogrammaselsalva(true)} );



var giorni=["DOM: ","LUN: ","MAR: ","MER: ","GIO: ","VEN: ","SAB: "];

var nbancoselez;  //banco 1-4 selezionato da form selezione programmi. Variabile interna al form
var nprogrammasel;  //posizioni in progranna[] da modificare
var noggettosel;

function correzionepiu(){
	
	if (programma[203]==255) {programma[203]=0; return;}
	if (programma[203]==127) return;
	programma[203]++;
	
}
function correzionemeno(){
	
	if (programma[203]==0) {programma[203]=255; return;}
	if (programma[203]==128) return;
	programma[203]--;
	
}

function isteresipiu(){
	
	if (programma[214]==255) return;
	programma[214]++;
	
}
function isteresimeno(){
	
	if (programma[214]==0) return;
	
	programma[214]--;
	
}



function ripristinaprogramma(){
	for (i=0;i<230;i++){      //L'ultimo is lo stato on/off
	  j=parseInt(copiadatiricevuti.slice(2+4*i),10);
	  programma[i]=j;
	} 

}

function selezionebottonesel(bottone){
	nbancoselez=bottone;   //programma 1-4 selezionato da form selezione programmi. Variabile interna al form
	for (i=0;i<4;i++) {
		document.getElementById("nprogrammasel"+i).style.backgroundColor='#aaaaaa';
	}
	document.getElementById("nprogrammasel"+bottone).style.backgroundColor='green';
}




function selezionaprogramma(nprogramma,oggetto){
	noggettosel=oggetto;
	
	oggetto.style.backgroundColor='green';
	nprogrammasel=nprogramma;
	selezionebottonesel(programma[nprogramma]);

	$("#nprogrammasel").show();
	
}

function nprogrammaselsalva(salva){
	$("#nprogrammasel").hide();
	noggettosel.style.backgroundColor='#cccccc';
	if (salva) {
		programma[nprogrammasel]=nbancoselez;
		refreshimpostazioni();
	}
}


function comandoimpostazioni(){
	if (programma[221]==0) document.getElementById("programmafisso").checked=true;
	else document.getElementById("programmasettimanale").checked=true;
	document.getElementById("eccezione").checked=programma[215];
	refreshcorrezioneisteresi()
	refreshimpostazioni();
	$("#impostazioni").show();
	
}

function impostazionisalva(){
	$("#impostazioni").hide();
	inviasettaggi();
}


function impostazioniannulla(){
	$("#impostazioni").hide();
	ripristinaprogramma();
}

function refreshimpostazioni(){
	document.getElementById("selprogrammafisso").textContent=descrizioneprogrammi[programma[222]];
	document.getElementById("selprogrammaeccezione").textContent=descrizioneprogrammi[programma[220]];
	for (i=0;i<7;i++){
		document.getElementById("programmasett"+i).innerHTML="<b>"+giorni[i]+"</b>"+descrizioneprogrammi[programma[223+i]];
	}
	
	var today = new Date();		
	var year = today.getFullYear();

	var day=programma[216];
	var month=programma[217];
	
	document.getElementById("datainizioeccezione").value=year + "-" + (month < 10 ? "0" + month : month) + "-" + (day < 10 ? "0" + day : day);

	day=programma[218];
	month=programma[219];

	
	document.getElementById("datafineeccezione").value=year + "-" + (month < 10 ? "0" + month : month) + "-" + (day < 10 ? "0" + day : day);
	
}

function cambiadatainizio(){
	var data=new Date(document.getElementById("datainizioeccezione").value);
	programma[216]=data.getDate();
	programma[217]=data.getMonth()+1;
	document.getElementById("datafineeccezione").value=document.getElementById("datainizioeccezione").value;
	
}
function cambiadatafine(){
	var data=new Date(document.getElementById("datafineeccezione").value);
	programma[218]=data.getDate();
	programma[219]=data.getMonth()+1;

	
}


function consegno(valore){  
	if (valore<128) return ("+"+valore/10);
	return ("-"+(256-valore)/10);
}

function refreshcorrezioneisteresi(){
	document.getElementById("correzionescritta").textContent=consegno(programma[203]);
	document.getElementById("isteresiscritta").textContent=programma[214]/10;
}



function visualizzaprogramma(banco){
	if (flaginviaannulla) {
		//se sono state fatte modifiche chiede di inviarle o annullarle prima di modificare la visualizzzazione
		window.alert("Accettare o annullare le modifiche prima di cambiare visualizzazione");
		return;
	}
	programmavisualizzato=banco;
	for (i=0;i<50;i++){      
	  j=programma[i+50*banco]//parseInt(datiricevuti.slice(programmaattivo*50+2+4*i),10);
	  posizioni[i]=j;
	  posizionibackup[i]=j;  
	} 

	tcrono.rinfrescaprogrammi();
	tcrono.initcrono();	
	
}


var vedibarra=false;
function gestiscivedibarra () {
	vedibarra=!vedibarra
	document.getElementById("vedibarra").checked=vedibarra;
	tcrono.initcrono();		
}


var vediumidita=false;
function gestiscivediumidita () {
	vediumidita=!vediumidita;
	document.getElementById("vediumidita").checked=vediumidita;
	tcrono.initcrono();		
}


var veditemperatura=true;
function gestisciveditemperatura () {
	veditemperatura=!veditemperatura;
	document.getElementById("veditemperatura").checked=veditemperatura;
	tcrono.initcrono();		
}



function selprogrammacambianome(){
	var pluto=document.getElementById("nomebanco").value;
	while (pluto.length<10) 
	pluto+=" ";
	descrizioneprogrammi[programmasogliedatx]=pluto;
	aggiornapulsantiselezione(programmasogliedatx);
}


function aggiornapulsantiselezione(programma) {

	
	programmasogliedatx=programma;
	for (i=0;i<4;i++){
		document.getElementById("selprogramma"+i).style.backgroundColor='#aaaaaa';
		document.getElementById("selprogrammascritta"+i).textContent=descrizioneprogrammi[i];
	}
	document.getElementById("selprogramma"+programmasogliedatx).style.backgroundColor="red";
	document.getElementById("nomebanco").value=descrizioneprogrammi[programmasogliedatx];	
	
}

function inviasettaggiesoglie(){
	if(!flaginviaannulla) return;
	
	
	aggiornapulsantiselezione(programmavisualizzato);


	$("#qualeprogramma").show();
	
}

function salvasettaggi(){
	$("#qualeprogramma").hide();
	//copia in programma[] i valori di posizioni[], che sono stati modificati. li copia anche in posizionibackup[]
	flaginviaannulla=false;
	for (i=0;i<50;i++){
		programma[programmasogliedatx*50+i]=posizioni[i];   //viene aggiornato il programma soglie indicato da programmasogliedatx;
		posizionibackup[i]=posizioni[i];
	}
	
	inviasettaggi();
	visualizzaprogramma(programmasogliedatx);
	tcrono.rinfrescaprogrammi();
} 

function inviasettaggi() {
	var pippo;
	for (j=0;j<230;j++){
		//espando programma in fileprog
		pippo=(programma[j]).toString().padStart(3, '0');
		fileprog[4*j+4]=pippo[0];
		fileprog[4*j+5]=pippo[1];
		fileprog[4*j+6]=pippo[2];
		//fileprog[4*j+7]=',';  //la virgola c'era già
		
	}

	for (i=0;i<4;i++){
		
		for (j=0;j<10;j++){
			fileprog[924+10*i+j]=descrizioneprogrammi[i][j]
		}
		
	document.getElementById("programmascritta"+i).textContent=descrizioneprogrammi[i];  //pulsanti sopra il grafico
	document.getElementById("nprogrammaselscritta"+i).textContent=descrizioneprogrammi[i];    //pulsanti form generico scelta programma
	document.getElementById("selprogrammascritta"+i).textContent=descrizioneprogrammi[i];   //pulsanti form scelta programma per salvataggio
	}

	copiadatiricevuti=fileprog.join("").slice(2);     //serve per i ripristini
	websocket.send(fileprog.join(""));// per convertirlo da array a string
	document.getElementById("invia").style.backgroundColor='#aaaaaa';
	document.getElementById("annulla").style.backgroundColor='#aaaaaa';
	document.getElementById("invia").style.color='black';
	document.getElementById("annulla").style.color='black';
}
	
	



function annullasettaggi() {
	$("#qualeprogramma").hide();
	if(!flaginviaannulla) return;
	flaginviaannulla=false;
	
	for(var i=0;i<50;i++){
		posizioni[i]=posizionibackup[i];
	}
	tcrono.rinfrescaprogrammi();
	tcrono.initcrono();
	
	document.getElementById("invia").style.backgroundColor='#aaaaaa';
	document.getElementById("annulla").style.backgroundColor='#aaaaaa';
	document.getElementById("invia").style.color='black';
	document.getElementById("annulla").style.color='black';
}




function initWebSocket() {
	console.log("Trying to open a websocket");
	websocket = new WebSocket(gateway);
	websocket.onopen = onOpen;
	websocket.onclose = onClose;
	websocket.onmessage = onMessage;
}
function onOpen(event) {
	 var msg = {
		type: "message",
		text: "sono un pirla",
		id:   26,
		date: Date.now(),
		tabella: [[1,2],[3,4]]
     };

    console.log('Connection opened');
    //websocket.send(JSON.stringify(msg));

}
function onClose(event) {
	console.log('Connection closed');
	//setTimeout(initWebSocket, 2000);
}

var temperatura = 21;
var umidita = 50;
var acceso = false;
var ora = 0;
var minuti=0;
var wkday= 0; //giorno della settimana
var dataricevuta='2000 01 01  00 00 00 0';


var ccw=canvascrono.width;
var cch=canvascrono.height;

var ccoffsetl=ccw*0.025;  //distanza in percentuale tra il margine sinistro e il primo cursore
var ccoffseth=cch*0.152;  // distanza in percentuale tra il fondo e la prima tacchetta della temperatura


var ccwcursore=ccw*0.018;  //dimensioni del cursore rispetto al canvas
var cchcursore=cch/11;


var ccstepcursore=ccw*0.03905;  //step orizzontale
//var ccrangecursore=cch*0.0069;  //range in percentuale del cursore verticale 
var ccrangecursore=cch*0.692;  //range in percentuale cursore verticale dalla prima tacchetta all'ultima

var fccselected=false;
var ccselected=0;
var	ccdownx=0;
var	ccdowny=0;
var	cccurrentx=0;
var	cccurrenty=0;
var ccposizionedown=0;	//temperatura corrispondente al punto di click

var newx,newy,oldx,oldy;
var drawcursori=false;
var inunora=true;       //indica se la programmazione deve essere fatta a blocchi di ore

const ledacceso = document.getElementById('ledacceso');
const quantotempo = document.getElementById('quantotempo');



var datoletto;
var fdatoletto=false;
var buftemperature=[[],[],[],[],[],[],[]];
var bufumidita=[[],[],[],[],[],[],[]];
var bufflags=[[],[],[],[],[],[],[]];
var indicedatadisplay=0;   //quale giorno visualizziamo
var flagselezionegiorno=false;
var flaginviaannulla=false;
var programmaattivo=0;   //quele programma giornaliero di misure (temperatura-umidita) is attivo.
var programmavisualizzato=0; //quale programma is visualizzato
var programmasogliedatx=0; //quale programma di soglie deve essere aggiornato

var copiadatiricevuti;  //il file di programmazione come is stato ricevuto
var fileprog=[];   //il file di programmazione ricevuto convertito in array;
var ffileprogricevuto=false;    //verrà settato alla prima ricezione di fileprog
var programma=[];    //per la descrizione vedi fileprog.txt
var descrizioneprogrammi=[[],[],[],[]];
var posizioni = [150,165,170,175,180,185,190,195,200,199,198,197,196,195,190,185,180,175,170,165,150,145,140,135,130,160,165,170,175,180,185,190,195,200,199,198,197,196,195,190,185,180,175,170,165,150,145,140,135,130];
var posizionibackup = [170,175,180,185,190,195,200,199,198,197,196,195,190,185,180,175,170,165,150,145,140,135,130,160,165,170,175,180,185,190,195,200,199,198,197,196,195,190,185,180,175,170,165,150,145,140,135,130,140,150];
var mintemp=120;  //minima temperatura in decimi di grado
var maxtemp=220; //massima temperatura in decimi di grado
var rangetemp=maxtemp-mintemp; //range temperatura in decimi di grado

var salvafont;
var salvastato;
var programmaricevuto;

function onMessage(event) {

	if (event.data instanceof Blob) {
		//console.log('Il dato is un blob');
    
		var reader = new FileReader();
		var datoattuale;

	    // Set the `onload` event handler for the FileReader
	    reader.onload = function() {
	      // The `result` property of the FileReader contains the contents of the blob
	      
		  //console.log(reader.result);
		  datoletto = reader.result;
		  //fdatoletto=true;
		  if((datoletto[0]=='T')&&(datoletto.length==1450)&&(datoletto.charCodeAt(1)<7)){    //controllo che il dato letto sia formalmente corretto
			  //buftemperature[datoletto.charCodeAt(1)]=datoletto;
			  for(j=0;j<1450;j++){
		        buftemperature[datoletto.charCodeAt(1)][j]=datoletto.charCodeAt(j);
			  }
			  fdatoletto=true;
		  } else{
			  if((datoletto[0]=='U')&&(datoletto.length==1450)&&(datoletto.charCodeAt(1)<7)){    //controllo che il dato letto sia formalmente corretto
			  //buftemperature[datoletto.charCodeAt(1)]=datoletto;
				  
				  for(j=0;j<1450;j++){
					datoattuale=datoletto.charCodeAt(j);
			        bufumidita[datoletto.charCodeAt(1)][j]=datoattuale;   //&0x7f;
					bufflags[datoletto.charCodeAt(1)][j]=datoattuale&0x80;
				  }
			 fdatoletto=true;
			 } 

			  
		  }
	      
	    };

	    // Start reading the blob as text
	    //reader.readAsText(event.data);
		reader.readAsBinaryString(event.data);
		tcrono.initcrono();

		
			
	} else {
    
    var comando=event.data[0];
	//var salvaselprogramma=(event.data[1])&0x0f;    //in lettura ilprimo dato contiene il programma selezioato asciizzato
    var datiricevuti=event.data.slice(2);
    //console.log(event.data+" comando: "+comando+" dati: "+datiricevuti);
	    switch(comando){
	    	case "B": 
				salvastato=datiricevuti;
				programmaricevuto=parseInt(datiricevuti.slice(0),10);
				if	(ffileprogricevuto&&(programmaricevuto!=programmaattivo)) {programmaattivo=programmaricevuto;tcrono.rinfrescaprogrammi()}
				programmaattivo=programmaricevuto;
				
			
				//(ffileprogricevuto&&(programmavisualizzato!=programmaattivo)) visualizzaprogramma(programmaattivo);
				break;
	
	    	case "T": 
	            document.getElementById("valoretemperatura").innerText=datiricevuti+" "+String.fromCharCode(0x00B0)+"C";
				temperatura=datiricevuti;
				gaugetemp.value=temperatura;
	            break;
	
	        case "D": 
	            //document.getElementById("valoredata").innerHTML=datiricevuti;
				ora=parseInt(datiricevuti.slice(11),10);
				minuti=parseInt(datiricevuti.slice(14),10);
				wkday=parseInt(datiricevuti.slice(20),10);
				dataricevuta=datiricevuti;
				
				document.getElementById("orologiodata").innerHTML=parseInt(datiricevuti.slice(8),10).toString().padStart(2, "0")+"/"+parseInt(datiricevuti.slice(5),10).toString().padStart(2, "0")+"/"+parseInt(datiricevuti.slice(0),10);
				document.getElementById("orologioora").innerHTML=parseInt(datiricevuti.slice(12),10).toString().padStart(2, "0")+":"+parseInt(datiricevuti.slice(15),10).toString().padStart(2, "0")+":"+parseInt(datiricevuti.slice(18),10).toString().padStart(2, "0");
				document.getElementById("day"+wkday).style.color='yellow';
				if (!flagselezionegiorno) {
					indicedatadisplay=wkday;
					document.getElementById("day"+wkday).style.backgroundColor='green';
				}
	            break;
			case "U": 
				document.getElementById("valoreumidita").innerText=datiricevuti+" %";
				umidita=datiricevuti;
				gaugeRH.value=umidita;
				break;
	        case "P":

				
			copiadatiricevuti=datiricevuti;
				for (j=0;j<event.data.length;j++){
					fileprog[j]=event.data[j];
				}
				
				
				ripristinaprogramma();
	           
				visualizzaprogramma(programmaattivo); //quando riceve il messaggio di programmazione aggiorna i dati del programma selezionato.
				ffileprogricevuto=true;
				gestiscionoff(false);
				gestisciautoman();
				tcrono.rinfrescaprogrammi();

	            break;
			
			case "A":   //acceso
	            acceso=true;
				ledacceso.style.backgroundColor = 'red';
				ledacceso.textContent="ACCESO";
	            break;
				
			case "S":  //spento
	            acceso=false;
				ledacceso.style.backgroundColor = 'black';
				ledacceso.textContent="SPENTO";
	            break;
	
	
	    	
	    	default: console.log("messaggio non riconosciuto");
	    }   //switch
	
	}  //else
	
	//tcrono.initcrono();

    
}

function log(msg) {
    if (window.console && log.enabled) {
        console.log(msg);
    } 
} // log   
log.enabled = true;

$('#canvascrono').mousedown(function (ev) {
	tcrono.cronodown(ev);
});

$('#canvascrono').mousemove(function (ev) {
	tcrono.cronomove(ev);
});  



$('#canvascrono').mouseup(function (ev) {
	tcrono.cronoup(ev);
});


$('#canvascrono').on("wheel", function(ev) {
	tcrono.cronowheel(ev.originalEvent.deltaY,ev.originalEvent.offsetX,ev.originalEvent.offsetY,ev.originalEvent.target);
});



//le coordinate 0,0 del canvas corrispondono all'angolo in alto a destra. Le coordinate y crescono dall'alto in basso



var tcrono = {
  start:function(){
    tcrono.inizializzazioni();
    tcrono.resetgrafici();
   
  },


    cronodown:function(ev){
    	var x  = (ev.offsetX || ev.clientX - $(ev.target).offset().left);
		var y  = (ev.offsetY || ev.clientY - $(ev.target).offset().top);
   	    //log("down "+x+" "+y);
		if (x<ccoffsetl) return;
		if (y<(cch-ccoffseth-ccrangecursore)) return;
		if(y>(cch-ccoffseth)) return;
   	    if (x>(ccoffsetl-ccstepcursore/3) && x<(ccoffsetl+24.66*ccstepcursore)){
   	        fccselected=true;
   	        //ccselected=parseInt((x+ccstepcursore/6-ccoffsetl)/(ccstepcursore/2));
			ccselected=parseInt((x-ccoffsetl)/(ccstepcursore/2));
   	        ccdownx=x;
   	        ccdowny=y;
   	        cccurrentx=x;
   	        cccurrenty=y;
   	        ccposizionedown=posizioni[ccselected];	
   	    } 
   	    tcrono.initcrono();
 
    },
    cronomove:function(ev){

			var x  = (ev.offsetX || ev.clientX - $(ev.target).offset().left);
			var y  = (ev.offsetY || ev.clientY - $(ev.target).offset().top);
			//log("move "+x+" "+y);

    	if (fccselected) {

            //calcola il delta posizione y in percentuale e la usa per aggiornare la posizione del cursore
            //il campo utile in verticale ha lo 0 in alto e cresce verso il basso.
            //ccoffseth corrisponde al 100% del valore
            //ccoffseth+ccrangecursore*100 corrisponde allo 0

            var delta = (y-ccdowny)/(ccrangecursore)*rangetemp;  //delta espressoin valore di temperatura
			var valore =Math.min(maxtemp, Math.max(mintemp, (ccposizionedown-delta)));
            posizioni[ccselected]=Math.round(valore);
            if ((ccselected&0xFE)==48) {
            	for (var i=0; i<50;i++) {
            		posizioni[i]=Math.round(valore);
            	}
            }if(inunora||drawcursori)  posizioni[ccselected^1]=Math.round(valore);
			//log("move "+x+" "+y + " " +  (y-ccdowny)+" "+ delta);

			tcrono.initcrono();
		}else if (!drawcursori) {

			tcrono.initcrono();
			if ((x>=ccoffsetl)&&(x<(ccoffsetl+ccstepcursore*24))&&(y<(cch-ccoffseth))&&(y>(cch-ccoffsetl-ccrangecursore))) {
				ctxmt.fillStyle="RGBA(256,256,0,1)";	
				ctxmt.fillRect(x,cch-ccoffseth,ccw/400,-ccrangecursore);

				ctxmt.fillStyle="RGBA(180,180,180,1)";	
				ctxmt.fillRect(ccoffsetl+ccstepcursore*10,cch-ccoffseth*1.35-ccrangecursore,ccstepcursore*6,-ccrangecursore/7);

				
				var contaminuti=parseInt((x-ccoffsetl)/(ccstepcursore*24)*1440);
				var puntaore= parseInt(contaminuti/60);
				var puntamezzore= parseInt(contaminuti/30);
				var puntaminuti=parseInt(contaminuti%60);
				var puntatemp=buftemperature[indicedatadisplay][contaminuti+10]/10;
				var puntaRH=(bufumidita[indicedatadisplay][contaminuti+10])&0x7f;

				ctxmt.fillStyle="black";ctxmt.fillText("Ore: "+puntaore + ":" +puntaminuti.toString().padStart(2, "0") ,ccoffsetl+ccstepcursore*10.5,cch-ccoffseth*1.8-ccrangecursore);
				ctxmt.fillStyle="green";ctxmt.fillText("temp: "+puntatemp+" "+String.fromCharCode(0x00B0)+"C",ccoffsetl+ccstepcursore*10.5,cch-ccoffseth*1.6-ccrangecursore);
				ctxmt.fillStyle="red";ctxmt.fillText("Soglia: "+posizioni[puntamezzore]/10 +" "+String.fromCharCode(0x00B0)+"C",ccoffsetl+ccstepcursore*10.5,cch-ccoffseth*1.4-ccrangecursore);
				ctxmt.fillStyle="blue";ctxmt.fillText("RH: "+puntaRH+ " %",ccoffsetl+ccstepcursore*13.5,cch-ccoffseth*1.6-ccrangecursore);
				
			}
		}

    },

    cronoup:function(ev){
    	var x  = (ev.offsetX || ev.clientX - $(ev.target).offset().left)/canvascrono.width;
		var y  = (ev.offsetY || ev.clientY - $(ev.target).offset().top)/canvascrono.height;
   	    //log("up "+x+" "+y);
		if(!fccselected) return;
   	    fccselected=false;
   	    tcrono.initcrono();
		flaginviaannulla=true;
		document.getElementById("invia").style.backgroundColor='yellow';
		document.getElementById("annulla").style.backgroundColor='yellow';
		/*document.getElementById("invia").style.color='yellow';
		document.getElementById("annulla").style.color='yellow';*/

    },


	cronowheel:function(deltaY,offsetX,offsetY,target){
		
		//var x  = (offsetX - $(target).offset().left)/canvascrono.width;
		//var y  = (offsetY- $(target).offset().top)/canvascrono.height;

   	    log("wheel "+ deltaY+ " x: "+offsetX+" y: "+offsetY);
		if (deltaY>0){
			//amplifica o scrolla in su
			if ((rangetemp>40)&&(offsetY<cch/5)){ //se sono nella parte alta riduco il valore massimo
				maxtemp-=10;
			} else{ //se sono nella parte bassa aumento il minimo
				if ((rangetemp>40)&&(offsetY>cch*4/5)){
					mintemp+=10;
				}else{ //se sono nella parte centrale scrollo verso l'alto
					mintemp+=10;
					maxtemp+=10;
				}
			}
			
		} else {
			//comprime
			if (offsetY<cch/5){ //se sono nella parte alta aumento il valore massimo
				if (maxtemp<500) maxtemp+=10;
			} else{ //se sono nella parte bassa diminuisco il minimo
				if (mintemp>0) {
					mintemp-=10;
					if (offsetY<cch*4/5) maxtemp-=10; //se sono nella parte centrale dello schermo scrollo
				}
			}

		}

		
   	    rangetemp=maxtemp-mintemp;
		tcrono.initcrono();
    },  


  resetgrafici:function(){

    setTimeout(tcrono.initcrono,1000);
    
  },


	


rinfrescaprogrammi:function(){
	for (i=0;i<4;i++){
		document.getElementById("programma"+i).style.backgroundColor='#aaaaaa';
		document.getElementById("programma"+i).style.color='black';
	}
	document.getElementById("programma"+programmavisualizzato).style.backgroundColor='green';
	document.getElementById("programma"+programmaattivo).style.color='yellow';
	if (ffileprogricevuto){
		for (i=0;i<4;i++){
			descrizioneprogrammi[i]="";
			for (j=0;j<10;j++){
				descrizioneprogrammi[i]+=fileprog[924+10*i+j];
			}
		document.getElementById("programmascritta"+i).textContent=descrizioneprogrammi[i];  //pulsanti sopra il grafico
		document.getElementById("nprogrammaselscritta"+i).textContent=descrizioneprogrammi[i];    //pulsanti form generico scelta programma
		document.getElementById("selprogrammascritta"+i).textContent=descrizioneprogrammi[i];   //pulsanti form scelta programma per salvataggio
		}
	}
},

  inizializzazioni: function(){
	  tcrono.rinfrescaprogrammi();
  },
		
  

	
initcrono:function() {
    
    
	    
		if($('input[name="tipodisplay"]:checked').val()=="cursori") drawcursori=true; else drawcursori=false;
		
		
		ctxmt.font="bold "+(18*ccw/1500)+"px Verdana";
		
		ctxmt.drawImage(tmcrono, 0, 0,ccw,cch);  //in caso di !drawcursori per vedere sotto la legenda ore
		if(!drawcursori){
			ctxmt.fillStyle="RGBA(128,128,128,1)";	
			ctxmt.fillRect(0,cch*0.88,ccw,-cch*0.88);
		}


	
		//scrive valori sull'asse y 
		for (var i=mintemp/10; i<=maxtemp/10;i++){
		    ctxmt.fillStyle="black";
			if ((i-mintemp)%4==3) ctxmt.fillStyle="brown";
			ctxmt.fillText(i,ccoffsetl*0.2,cch-ccoffseth*0.93-(ccrangecursore/rangetemp)*(i*10-mintemp));

			//disegna righe di temperatura
			if (!drawcursori) {
				var spessore=-ccrangecursore/400;
				if ((i%5)==0) spessore*=2;
				ctxmt.fillStyle="RGBA(80,80,80,1)";
				if ((i%5)==0) ctxmt.fillStyle="RGBA(0,0,0,1)";
				ctxmt.fillRect(ccoffsetl,cch-ccoffseth-(i-mintemp/10)*ccrangecursore/(rangetemp/10),ccoffsetl+ccstepcursore*24,spessore); 

			}
	    }
	
	
	
	
		if(!drawcursori) ctxmt.beginPath(); // Start a new path per scaletta impostazioni

		
		for (var i=0; i<50;i++){
		    tcrono.drawcursore(i,posizioni[i]);  
		}

		
		if(!drawcursori) {
			//DISEGNA SCALETTA IMPOSTAZIONI
			ctxmt.strokeStyle="RGBA(255,0,0,1)";
		    ctxmt.lineWidth=ccstepcursore/20;
			ctxmt.stroke(); // Render the path
		}

	



	

		//disegna grafico temperatura se stato letto almeno una volta
		if (fdatoletto){
			 //fdatoletto=false;
			 ctxmt.beginPath(); // Start a new path
			 //ctxmt.moveTo(ccoffsetl,cch-ccoffseth); // inizializza la penna
			  
			 var zeriprima=true; //se ci sono degli zeri prima del primo valore li tollera
		    for (var j=10;j<1450;j++){     //il buffer ha 10 bytes di intestazione
				  var newx=ccoffsetl+(j-10)*24*ccstepcursore/1440;
				  //var newdato=datoletto.charCodeAt(j);
				  var newdato=buftemperature[indicedatadisplay][j];
				  //var newdato=buftemperature[wkday].charCodeAt(j);
				  if (newdato==0){

					  if (!zeriprima) 
							break;
				  } else {
					  newy=cch-ccoffseth-(newdato-mintemp)/rangetemp*ccrangecursore;
					  if(zeriprima) {
						  zeriprima=false; 
						  ctxmt.moveTo(newx,newy);  //inizializza la penna
					  }
					  else ctxmt.lineTo(newx,newy);
					  
				  }
				  
			  }
				
			  ctxmt.strokeStyle="RGBA(0,255,0,1)";
			  ctxmt.lineWidth=ccrangecursore/200;
			  if(veditemperatura) ctxmt.stroke(); // Render the path


			  //ora disegno l'umidita e la barra acceso/spento
			  ctxmt.fillStyle="black";	
			  ctxmt.fillRect(ccoffsetl,cch-ccoffseth*0.8,ccoffsetl+ccstepcursore*24,cch/100);  //inizializzo la barra acceso/spento nera
			  
			  ctxmt.beginPath(); // Start a new path
			  //ctxmt.moveTo(ccoffsetl,cch-ccoffseth); // inizializza la penna
			  
			  var inizioacceso=0;
			  var eraacceso=0;

			  function disegnarettrosso(){
				  
				 ctxmt.fillStyle="red";     //per i rettangoli rossi							  
				 ctxmt.fillRect(ccoffsetl+inizioacceso*24*ccstepcursore/1440,cch-ccoffseth*0.8,(j-10-inizioacceso)*24*ccstepcursore/1440,cch/100); 
				 ctxmt.fillStyle="RGBA(255,0,0,0.1)";   //per i rettangoli grandi trasparenti				  
				 ctxmt.fillRect(ccoffsetl+inizioacceso*24*ccstepcursore/1440,cch-ccoffseth,(j-10-inizioacceso)*24*ccstepcursore/1440,-ccrangecursore); 
						  		  

			  }


			  var contaacceso=0;
			  var zeriprima=true; //se ci sono degli zeri prima del primo valore li tollera
			  for (var j=10;j<1450;j++){     //il buffer ha 10 bytes di intestazione
				  var newx=ccoffsetl+(j-10)*24*ccstepcursore/1440;
				  //var newdato=datoletto.charCodeAt(j);
				  var newdato=bufumidita[indicedatadisplay][j];
				  //var newdato=buftemperature[wkday].charCodeAt(j);
				  if ((newdato&0x7f)==0){

					  if (!zeriprima) 
							break;
				  } else {
					  newy=cch-ccoffseth-(newdato&0x7f)/100*ccrangecursore;
					  if(zeriprima) {
						  zeriprima=false; 
						  ctxmt.moveTo(newx,newy);  //inizializza la penna
					  }
					  else ctxmt.lineTo(newx,newy);
					  
				  }
				  if ((newdato&0x80)!=0){  //nuovo dato is acceso
					  contaacceso++;
						if (eraacceso==0) { //e prima era spento inizializza il rettangolo
							inizioacceso=j-10;
							eraacceso=1;
						}
					  
				  } else{  //nuovo dato is spento
					  if (eraacceso!=0)   {  //e prima era acceso disegno rettangolo rosso
						  disegnarettrosso();
						  eraacceso=0;
					  
					  }
				  }
				  
			  }
			  if (eraacceso!=0) 	disegnarettrosso();     //ultimo rettangolo finale

			  
			  ctxmt.strokeStyle="RGBA(0,0,255,1)";
			  ctxmt.lineWidth=ccrangecursore/200;
			  if (vediumidita) ctxmt.stroke(); // Render the path
			  var quanteore =parseInt(contaacceso/60);

			  quantotempo.textContent="ACCESO OGGI "+quanteore+":"+(contaacceso-quanteore*60).toString().padStart(2, "0");

			//se abilitato disegna linea verticale in corrispondenza all'ora attuale
			if (vedibarra) {
				ctxmt.strokeStyle="RGBA(255,255,255,1)";
				ctxmt.lineWidth=ccrangecursore/200;
				ctxmt.beginPath();
				ctxmt.setLineDash([2, 2]);
				var posizionex =ccoffsetl+(ora*60+minuti)*24*ccstepcursore/1440;
				ctxmt.moveTo(posizionex,cch-ccoffseth*0.8);
				ctxmt.lineTo(posizionex,cch-ccoffseth-ccrangecursore);
				ctxmt.stroke();
				ctxmt.setLineDash([]);
				
				//Scrive il valore della soglia attuale e della temperatura attuale
				salvafont=ctxmt.font;
				ctxmt.font="bold "+(35*ccw/1500)+"px Verdana";
				ctxmt.fillStyle="red";
				var valoresoglia=posizioni[Math.floor((ora*60+minuti)/30)]/10;
				if (programma[201]!=0) {  //Se siamo in manuale scrive il valore dell'impostazione manuale
					ctxmt.fillStyle="green";
					valoresoglia=programma[202]/10;
				}
				var valoretemp=parseFloat(temperatura);
				var altezzatemp=cch*0.87;
				var altezzasoglia=cch*0.87;
				if (valoretemp>valoresoglia){
					altezzatemp=cch*0.83; 
				} else altezzasoglia=cch*0.83;
				ctxmt.fillText("S:"+valoresoglia.toFixed(1) +" "+String.fromCharCode(0x00B0)+"C",posizionex+ccstepcursore/5,altezzasoglia);
				ctxmt.strokeStyle="black";
				ctxmt.lineWidth=ccrangecursore/400;
				ctxmt.strokeText("S:"+valoresoglia.toFixed(1) +" "+String.fromCharCode(0x00B0)+"C",posizionex+ccstepcursore/5,altezzasoglia);
				ctxmt.fillStyle="RGBA(0,255,0,1)";
				ctxmt.fillText("T:"+valoretemp.toFixed(1) +" "+String.fromCharCode(0x00B0)+"C",posizionex+ccstepcursore/5,altezzatemp);
				ctxmt.strokeText("T:"+valoretemp.toFixed(1) +" "+String.fromCharCode(0x00B0)+"C",posizionex+ccstepcursore/5,altezzatemp);
				ctxmt.font=salvafont;
			}

			if (programma[201]!=0){ //se siamo in manuale disegna riga di livello verde  per temp manuale
				
				ctxmt.fillStyle="green";
				ctxmt.fillRect(0,cch-ccoffseth*1.03-(programma[202]-mintemp)/rangetemp*ccrangecursore,ccw,cch/200);
			}
	
		
			//disegna valore in corrispondenza al cursore selezionato
			
			if(fccselected){
				ctxmt.fillStyle="yellow";
				var sinistra=ccoffsetl+ccstepcursore/2*(ccselected+0.7);
				var alto=cch-ccoffseth*1.2-(posizioni[ccselected]-mintemp)/rangetemp*ccrangecursore;
				ctxmt.fillRect(sinistra,alto,ccw/10,-cch/10);
				ctxmt.fillStyle="black";
				
				var quantiminuti=0;
				var quanteore=ccselected/2;
				if ((ccselected&1)==1){
					if ((!inunora)&&(!drawcursori)) quantiminuti=30;
					quanteore=(ccselected-1)/2;
				}
				
				
				ctxmt.fillText("ORE: "+quanteore.toString().padStart(2, "0")+":"+quantiminuti.toString().padStart(2, "0"),sinistra+ccstepcursore*0.2,alto-cch*0.08);
				salvafont=ctxmt.font;
	            ctxmt.font="bold "+(40*ccw/1500)+"px Verdana";
				if ((Math.round(posizioni[ccselected])%10)==0) ctxmt.fillStyle="red";
				ctxmt.fillText((posizioni[ccselected]/10).toFixed(1),sinistra+ccstepcursore*0.2,alto-cch*0.02);
				ctxmt.font=salvafont;
			}
	
			  
		  }

   },


	drawcursore:function(n,p) {
    	//n numero cursore 0-23
    	//p temperatura in decimi di grado
    	//scrive su canvascrono


		
		function disegnacursore(imgcursore){
			if (drawcursori&&((n&1)==0)) ctxmt.drawImage(imgcursore,ccoffsetl+ccstepcursore*n/2,cch-ccoffseth-ppercento*ccrangecursore/100-cchcursore/2,ccwcursore,cchcursore);
			else {
				//disegna scaletta impostazioni
				newx=ccoffsetl+ccstepcursore/2*(n+1);
				newy=cch-ccoffseth-ppercento*ccrangecursore/100;
				if (n==0) {
					ctxmt.moveTo(ccoffsetl,newy);  //inizializza la penna per scaletta impostazioni
				} else {
					ctxmt.lineTo(oldx,newy);  //gradino verticale
				}
				ctxmt.lineTo(newx,newy);  //gradino orizzontale
				oldx=newx;
			}
			//disegna linea verticale griglia ore
			if (!drawcursori&&((n&1)==0)){
				ctxmt.fillStyle="RGBA(80,80,80,1)"
				ctxmt.fillRect(ccoffsetl+ccstepcursore/2*(n),cch-ccoffseth,ccw/400,-ccrangecursore);
			}
		}

 
		
		var ppercento=(p-mintemp)/(rangetemp)*100; //ppercento posizione percentuale della corsa

		
		ctxmt.fillStyle="green";
    	var intero=false;
 
		if ((Math.round()%10)==0) intero=true;

		

    	if (fccselected && (n==ccselected)){   //disegna riga di livello
 
    	    if (intero) ctxmt.fillStyle="red"; 
  
			ctxmt.fillRect(0,cch-ccoffseth*1.02-ppercento*ccrangecursore/100,ccw,cch/200);

			disegnacursore(tmcursoresel);

		
			
			
			

	    }
	    else{
	    	if (n==48){
 				disegnacursore(tmcursoreblu);

	    	} else {
                disegnacursore(tmcursore);
	    	}
	    }        
		ctxmt.fillStyle="black";
		
		//scrive la temperatura in cima al cursore
		if (drawcursori) {
			if ((n&1)==0) ctxmt.fillText((p/10).toFixed(1),ccoffsetl*0.8+ccstepcursore/2*n,cch*0.07);
		} else {
			if ((n&1)==0){
				ctxmt.fillText((p/10).toFixed(1),ccoffsetl*1.2+ccstepcursore/2*n,cch*0.15);//temperature pari
			} else {
				if (!inunora) ctxmt.fillText((p/10).toFixed(1),ccoffsetl*1.2+ccstepcursore/2*n,cch*0.13);//temperature dispari shiftate in alto
			}
		}
       
    },



}

function aggiustavista() {
    canvascrono = document.getElementById("canvascrono");
    ctxmt = canvascrono.getContext('2d');
    canvascrono.width=    $("#cursoridiv").width();
    canvascrono.height=$("#cursoridiv").height();
    tcrono.resetgrafici();
/*    canvascrono.width=    $("#cursoridiv").width();
    canvascrono.height=$("#cursoridiv").height();
	    tcrono.initcrono,100,ctxmt,canvascrono.width,canvascrono.height */
}  

$(document) .ready(function () {

  tcrono.start();

}); 