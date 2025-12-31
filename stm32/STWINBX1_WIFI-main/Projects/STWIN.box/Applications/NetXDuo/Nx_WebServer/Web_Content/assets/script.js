/*------------------------------------------------------------------
* Bootstrap Simple Admin Template
* Version: 2.1
* Author: Alexis Luna
* Website: https://github.com/alexis-luna/bootstrap-simple-admin-template
-------------------------------------------------------------------*/
// Toggle sidebar on Menu button click
$('#sidebarCollapse').on('click', function () {
    $('#sidebar').toggleClass('active');
    $('#body').toggleClass('active');
});

var tx_url = "/GetTXData";
var nx_url = "/GetNXData";
var mems_url = "/GetMemsData";
function loadData() {
    jQuery.get(tx_url, function (data, status) {
        var array = data.split(',');
        var content =
        document.getElementById("tx_active").innerHTML = "Resumptions : " + array[0];
        document.getElementById("tx_suspended").innerHTML = "Suspentions : " + array[1];
        document.getElementById("idle_returns").innerHTML = "Idle Returns : " + array[2];
        document.getElementById("non_idle_returns").innerHTML = "Non Idle returns : " + array[3];
    });

    jQuery.get(nx_url, function (data, status) {
        var array = data.split(',');
        /* Server returns: received,sent,connections,disconnections */
        document.getElementById("nx_received").innerHTML = "Total Bytes Received  :  " + array[0];
        document.getElementById("nx_sent").innerHTML = "Total Bytes Sent  : " + array[1];
        document.getElementById("nx_connect").innerHTML = "Total connections  : " + array[2];
        document.getElementById("nx_disconnect").innerHTML = "Total Disconnections  :  " + array[3];


    });

    jQuery.get(mems_url, function (data, status) {
        if (!data || data === "NA") {
            document.getElementById("mems_status").innerHTML = "Status : Waiting for first packet...";
            return;
        }

        var a = data.split(',');
        /* CSV:
         * 0 seq
         * 1 timestamp_ms
         * 2 uptime_sec
         * 3 rms_raw
         * 4 spl_db
         * 5 peak_amplitude
         * 6 zcr_rate
         * 7 status_flags
         * 8 error_count
         * 9-16 fft_band[0..7]
         */

        document.getElementById("mems_status").innerHTML = "Status : Running";
        document.getElementById("mems_seq").innerHTML = "Seq : " + a[0];
        document.getElementById("mems_uptime").innerHTML = "Uptime (s) : " + a[2];
        document.getElementById("mems_rms").innerHTML = "RMS : " + a[3];
        document.getElementById("mems_spl").innerHTML = "SPL (dB) : " + a[4];
        document.getElementById("mems_peak").innerHTML = "Peak : " + a[5];
        document.getElementById("mems_zcr").innerHTML = "ZCR : " + a[6];
        document.getElementById("mems_err").innerHTML = "Errors : " + a[8];

        document.getElementById("mems_fft0").innerHTML = "B0 : " + a[9];
        document.getElementById("mems_fft1").innerHTML = "B1 : " + a[10];
        document.getElementById("mems_fft2").innerHTML = "B2 : " + a[11];
        document.getElementById("mems_fft3").innerHTML = "B3 : " + a[12];
        document.getElementById("mems_fft4").innerHTML = "B4 : " + a[13];
        document.getElementById("mems_fft5").innerHTML = "B5 : " + a[14];
        document.getElementById("mems_fft6").innerHTML = "B6 : " + a[15];
        document.getElementById("mems_fft7").innerHTML = "B7 : " + a[16];
    });

    var t = setTimeout(function () { loadData() }, 3000);
}

var led_request = "/LedToggle";
function toggleLed() {

    var led = document.getElementById("greenLed");
    var label = document.getElementById("buttonLabel");
    if(led.checked){
        label.innerHTML = "On";
        jQuery.post("/LedOn");

    }
    else
    {
        label.innerHTML = "Off";
        jQuery.post("/LedOff");
    }
}
