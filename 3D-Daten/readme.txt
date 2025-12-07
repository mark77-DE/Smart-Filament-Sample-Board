<button onclick="setLED()">LED 5 Rot</button>
<script>
function setLED(){
  fetch("http://192.168.30.125/json/state", {
    method: "POST",
    headers: {"Content-Type": "application/json"},
    body: JSON.stringify({
      seg: [{ i: [5, [255,0,0]] }]
    })
  });
}
</script>
