import React from "react";
import { Switch, Route} from 'react-router-dom'; // import the react-router-dom components
import Home from './Home.jsx'
import OtaUpdate from './OtaUpdate.jsx'

const Main = () => (
  <main>
    <Switch>
      <Route exact path='/' component={Home} />
      <Route exact path='/ota' component={OtaUpdate} />
    </Switch>
  </main>
);

export default Main;