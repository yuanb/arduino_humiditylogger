function getDataFilename(str) {
	point = str.lastIndexOf("file=") + 4;

	tempString = str.substring(point + 1, str.length)
	if (tempString.indexOf("&") == -1) {
		return (tempString);
	} else {
		return tempString.substring(0, tempString.indexOf("&"));
	}
}

query = window.location.search;

var dataFilePath = "/data/" + getDataFilename(query);
var currentFilePath = "/current.jso";

$(function() {
	var chart, chart_currrentHumidity, chart_currentTemperature;
	Highcharts.setOptions({
		global: {
			timezoneOffset: 4 * 60
		}
	});

	$(document).ready(function() {
		// define the options
		var options = {
			chart: {
				renderTo: 'container',
				zoomType: 'x',
				spacingRight: 20,
				shadow: true,
				type: 'spline'
			},
			title: {
				text: 'Humidity levels recorded by Arduino'
			},

			subtitle: {
				text: 'Click and drag in the plot area to zoom in'
			},

			xAxis: {
				type: 'datetime',
				dateTimeLabelFormats: {
					hour: '%I:%M %P',
					minute: '%I %M'
				},
				maxZoom: 2 * 3600000,
			},

			yAxis: [{
				title: {
					text: 'Humidity reading (0 - 100)'
				},
				min: 0,
				max: 100,
				tickInterval: 20,
				minorTickInterval: 10,
				minorTickWidth: 2,
				minorTickLength: 4,
				lineWidth: 2,
				tickWidth: 3,
				tickLength: 6,
				startOnTick: false,
				showFirstLabel: false,
				gridLineColor: '#8AB8E6',
				alternateGridColor: {
					linearGradient: {
						x1: 0,
						y1: 1,
						x2: 1,
						y2: 1
					},
					stops: [
						[0, '#FAFCFF'],
						[0.5, '#F5FAFF'],
						[0.8, '#E0F0FF'],
						[1, '#D6EBFF']
					]
				}
			}, {
				title: {
					text: 'temperature reading(0 - 40)'
				},
				min: 0,
				max: 45,
				tickInterval: 10,
				minorTickInterval: 5,
				minorTickWidth: 2,
				minorTickLength: 4,
				lineWidth: 2,
				tickWidth: 3,
				tickLength: 6,
				startOnTick: false,
				showFirstLabel: false,
				opposite: true
			}],

			legend: {
				enabled: true,
				align: 'right',
				verticalAlign: 'middle',
				layout: 'vertical',
				borderWidth: 0
			},

			credits: {
				enabled: true,
				position: {
					align: 'right',
					x: -10,
					y: -10
				},
				href: "https://github.com/yuanb/arduino_humiditylogger",
				style: {
					color: 'blue'
				},
				text: "Arduino humidity logger"
			},

			tooltip: {
				valueSuffix: '%',
				formatter: function() {
					return '<b>' + this.series.name + '</b><br/>' +
						Highcharts.dateFormat('%H:%M - %b %e, %Y', this.x) + ': ' + this.y + '%';
				}
			},

			plotOptions: {
				series: {
					cursor: 'pointer',
					lineWidth: 1.0,
					point: {
						events: {
							click: function() {
								hs.htmlExpand(null, {
									pageOrigin: {
										x: this.pageX,
										y: this.pageY
									},
									headingText: this.series.name,
									maincontentText: Highcharts.dateFormat('%H:%M - %b %e, %Y', this.x) + ':<br/> ',
									width: 200
								});
							}
						}
					},
				}
			},

			series: [{
				name: 'Relative Humidity %',
				marker: {
					radius: 2
				},
				yAxis: 0
			}, {
				name: 'Temperature in °C',
				color: '#FF00FF',
				marker: {
					radius: 2
				},
				yAxis: 1,
			}]
		};

		jQuery.get(dataFilePath, null, function(csv, state, xhr) {
			var date, humidity, temperature;

			// set up the two data series
			humidityLevels = [];
			temperatureLevels = [];

			// inconsistency
			if (typeof csv !== 'string') {
				csv = xhr.responseText;
			}

			// split the data return into lines and parse them
			csv = csv.split(/\n/g);
			jQuery.each(csv, function(i, line) {

				if (line.length == 0) {
					return true;
				}
				// all data lines start with a double quote
				line = line.split(',');
				date = parseInt(line[0], 10) * 1000;
				humidity = parseInt(line[1], 10);
				temperature = parseInt(line[2], 10);
				//filter
				if (humidity >= 0 && humidity <= 100) {
					humidityLevels.push([
						date,
						humidity
					]);
				}
				if (temperature >= -20 && temperature <= 80) {
					temperatureLevels.push([
						date,
						temperature
					]);
				}
			});

			options.series[0].data = humidityLevels;
			options.series[1].data = temperatureLevels;

			chart = new Highcharts.Chart(options);
		});

		//2nd chart
		var options_humidity = {
			chart: {
				type: 'gauge',
				renderTo: 'currentHumidity_div',
				plotBackgroundColor: null,
				plotBackgroundImage: null,
				plotBorderWidth: 0,
				plotShadow: false
			},

			title: {
				text: 'Current humidity'
			},

			pane: {
				startAngle: -150,
				endAngle: 150,
				background: [{
					backgroundColor: {
						linearGradient: { x1: 0, y1: 0, x2: 0, y2: 1 },
						stops: [
							[0, '#FFF'],
							[1, '#333']
						]
					},
					borderWidth: 0,
					outerRadius: '109%'
				}, {
					backgroundColor: {
						linearGradient: { x1: 0, y1: 0, x2: 0, y2: 1 },
						stops: [
							[0, '#333'],
							[1, '#FFF']
						]
					},
					borderWidth: 1,
					outerRadius: '107%'
				}, {
					// default background
				}, {
					backgroundColor: '#DDD',
					borderWidth: 0,
					outerRadius: '105%',
					innerRadius: '103%'
				}]
			},

			// the value axis
			yAxis: {
				min: 0,
				max: 100,

				minorTickInterval: 'auto',
				minorTickWidth: 1,
				minorTickLength: 10,
				minorTickPosition: 'inside',
				minorTickColor: '#666',

				tickPixelInterval: 30,
				tickWidth: 2,
				tickPosition: 'inside',
				tickLength: 10,
				tickColor: '#666',
				labels: {
					step: 2,
					rotation: 'auto'
				},
				title: {
					text: '%'
				},
				plotBands: [{
					from: 0,
					to: 25,
					color: '#DDDF0D' // yellow
				}, {
					from: 25,
					to: 70,
					color: '#55BF3B' // green
				}, {
					from: 70,
					to: 100,
					color: '#DF5353' // red
				}]
			},

			series: [{
				name: 'Speed',
				data: [80],
				tooltip: {
					valueSuffix: ' %'
				}
			}]
		};
		
		currentHumidity = [];
		$.getJSON(currentFilePath, function(data) {
			humidity = data['currentHumidity'];
			currentHumidity.push([humidity]);
		});

		jQuery.get(currentFilePath, null, function(current, state, xhr) {
			options_humidity.series[0].data = currentHumidity;
			chart_currrentHumidity = new Highcharts.Chart(options_humidity);
		});

		// 3rd chart
		var options_temperature = {
			chart: {
				type: 'gauge',
				renderTo: 'currentTemperature_div',
				plotBackgroundColor: null,
				plotBackgroundImage: null,
				plotBorderWidth: 0,
				plotShadow: false
			},

			title: {
				text: 'Current temperature'
			},

			pane: {
				startAngle: -150,
				endAngle: 150,
				background: [{
					backgroundColor: {
						linearGradient: { x1: 0, y1: 0, x2: 0, y2: 1 },
						stops: [
							[0, '#FFF'],
							[1, '#333']
						]
					},
					borderWidth: 0,
					outerRadius: '109%'
				}, {
					backgroundColor: {
						linearGradient: { x1: 0, y1: 0, x2: 0, y2: 1 },
						stops: [
							[0, '#333'],
							[1, '#FFF']
						]
					},
					borderWidth: 1,
					outerRadius: '107%'
				}, {
					// default background
				}, {
					backgroundColor: '#DDD',
					borderWidth: 0,
					outerRadius: '105%',
					innerRadius: '103%'
				}]
			},

			// the value axis
			yAxis: {
				min: 0,
				max: 45,

				minorTickInterval: 'auto',
				minorTickWidth: 1,
				minorTickLength: 10,
				minorTickPosition: 'inside',
				minorTickColor: '#666',

				tickPixelInterval: 30,
				tickWidth: 2,
				tickPosition: 'inside',
				tickLength: 10,
				tickColor: '#666',
				labels: {
					step: 2,
					rotation: 'auto'
				},
				title: {
					text: '°C'
				},
				plotBands: [{
					from: 0,
					to: 10,
					color: '#DDDF0D' // yellow
				}, {
					from: 10,
					to: 35,
					color: '#55BF3B' // green
				}, {
					from: 35,
					to: 45,
					color: '#DF5353' // red
				}]
			},

			series: [{
				name: 'Speed',
				data: [80],
				tooltip: {
					valueSuffix: ' °C'
				}
			}]
		};
		
		currentTemperature = [];
		$.getJSON(currentFilePath, function(data) {
			temperature = data['currentTemperature'];
			currentTemperature.push([temperature]);
		});
		jQuery.get(currentFilePath, null, function(current, state, xhr) {
			options_temperature.series[0].data = currentTemperature;
			chart_currentTemperature = new Highcharts.Chart(options_temperature);
		});			

	});
});	
