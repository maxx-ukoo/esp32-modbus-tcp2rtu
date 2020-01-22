import React from "react";
import { Switch, Route, Link } from 'react-router-dom'; // import the react-router-dom components
import Home from './Home.jsx'
import Modbus from './Modbus.jsx'
import Io from './Io.jsx'

const Main = () => (
  <main>
    <Switch>
      <Route exact path='/' component={Home} />
      <Route exact path='/modbus' component={Modbus}/>
      <Route exact path='/io' component={Io} />
    </Switch>
  </main>
);

export default Main;