<div class="col-sm-12">
    <div class="panel panel-default">
        <div class="panel-heading">
            <h3 class="panel-title"><?php echo $title; ?></h3>
        </div>
        <div class="panel-body">
            <?php
            if (empty($stat)) {
                echo Yii::t('admin', 'Statistics not avaiable');
            } else {
                Yii::import('ext.charts.*');
                $chart = new Highchart();
                $chart->tooltip->formatter = new HighchartJsExpr("function() { return this.y + ' : ' + this.x;}");
                $chart->printScripts();
                $i = 0;
                foreach ($stat as $key => $value) {
                    if ($key == 'time') {
                        continue;
                    }
                    echo '<div id="container' . $i . '"></div>';
                    $chart->chart = array(
                        'renderTo' => 'container' . $i,
                        'type' => 'line',
                        'marginRight' => 130,
                        'marginBottom' => 25
                    );

                    $chart->title = array(
                        'text' => $key,
                        'x' => -20
                    );

                    $chart->xAxis->categories = $stat['time'];

                    $chart->yAxis = array(
                        'title' => array(
                            'text' => Yii::t('admin', 'Value')
                        ),
                        'plotLines' => array(
                            array(
                                'value' => 0,
                                'width' => 1,
                                'color' => '#808080'
                            )
                        )
                    );
                    $chart->legend = array(
                        'layout' => 'vertical',
                        'align' => 'right',
                        'verticalAlign' => 'top',
                        'x' => -10,
                        'y' => 100,
                        'borderWidth' => 0
                    );
                    $chart->series = array();
                    $chart->series[] = array(
                        'name' => 'min',
                        'data' => $value['min']
                    );
                    $chart->series[] = array(
                        'name' => 'max',
                        'data' => $value['max']
                    );
                    $chart->series[] = array(
                        'name' => 'avg',
                        'data' => $value['avg']
                    );

                    echo '<script type="text/javascript">' . $chart->render("chart") . '</script>';
                    $i++;
                    //break;
                }
                //echo '<script type="text/javascript">chart1 = new Highcharts.Chart({"chart":{"renderTo":"container","type":"line","marginRight":130,"marginBottom":25},"title":{"text":"Monthly Average Temperature","x":-20},"subtitle":{"text":"Source: WorldClimate.com","x":-20},"xAxis":{"categories":["Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"]},"yAxis":{"title":{"text":"Temperature (\u00b0C)"},"plotLines":[{"value":0,"width":1,"color":"#808080"}]},"legend":{"layout":"vertical","align":"right","verticalAlign":"top","x":-10,"y":100,"borderWidth":0},"series":[{"name":"Tokyo","data":[7,6.9,9.5,14.5,18.2,21.5,25.2,26.5,23.3,18.3,13.9,9.6]},{"name":"New York","data":[-0.2,0.8,5.7,11.3,17,22,24.8,24.1,20.1,14.1,8.6,2.5]},{"name":"Berlin","data":[-0.9,0.6,3.5,8.4,13.5,17,18.6,17.9,14.3,9,3.9,1]},{"name":"London","data":[3.9,4.2,5.7,8.5,11.9,15.2,17,16.6,14.2,10.3,6.6,4.8]}],"tooltip":{"formatter":function() { return \'<b>\'+ this.series.name +\'</b><br/>\'+ this.x +\': \'+ this.y +\'°C\';}}});</script>';
            }
            ?>
        </div>
    </div>
</div>