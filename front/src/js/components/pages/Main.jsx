import React from "react";
import { Switch, Route, Link } from 'react-router-dom'; // import the react-router-dom components
import Home from './Home.jsx'
import Modbus from './Modbus.jsx'
import Gpio from './Gpio.jsx'
import Mqtt from './Mqtt.jsx'
import OtaUpdate from './OtaUpdate.jsx'

const Main = () => (
  <main>
    <Switch>
      <Route exact path='/' component={Home} />
      <Route exact path='/modbus' component={Modbus}/>
      <Route exact path='/io' component={Gpio} />
      <Route exact path='/mqtt' component={Mqtt} />
      <Route exact path='/ota' component={OtaUpdate} />
    </Switch>
  </main>
);

export default Main;